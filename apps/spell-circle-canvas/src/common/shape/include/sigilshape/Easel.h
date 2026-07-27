#pragma once

/** @file
 * SigilShape easel — the artist surface. Everything underneath
 * (Ops/Blend/Curves/Points/Materials) is exact and explicit; this
 * header is the layer you REACH FOR: stock shapes, fluent chains, loud
 * defaults, one draw() at the end. Made for the sketch host — every
 * call reads like a sentence and every number is a dial worth turning.
 *
 *   easel::shape(easel::star(6, 90))
 *       .bloat(0.3f).roughen(3).offset(6)
 *       .fill({1, 0.6f, 0.2f, 1})
 *       .draw(canvas, {320, 240});
 *
 *   easel::blend(easel::star(5, 70), easel::dot(58))
 *       .colors(coral, sky).steps(9).smooth()
 *       .draw(canvas);
 *
 *   easel::wire({{-300,0,-100}, {0,120,100}, {300,0,-100}})
 *       .tube(9).draw(canvas, camera, viewport);
 *
 *   easel::particles().on(myWire).count(300).drift(24)
 *       .ramp(cyan, magenta).glow(canvas, camera, viewport);
 *
 * Recipes stay non-destructive: a Shape/Blend/Wire/Particles object is
 * a VALUE holding its dials; draw() (or path()/mesh()/cloud()) cooks
 * on demand. Copy one, tweak the copy, keep both.
 */

#include "sigilshape/Blend.h"
#include "sigilshape/Curves.h"
#include "sigilshape/Materials.h"
#include "sigilshape/Mesh.h"
#include "sigilshape/Ops.h"
#include "sigilshape/Points.h"
#include "sigilshape/Space.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <optional>
#include <vector>

namespace sigil::shape::easel {

// ---------------------------------------------------------------------------
// Stock shapes, centered on the origin — position at draw().

inline SkPath dot(float radius) { return SkPath::Circle(0, 0, radius); }

inline SkPath ngon(int sides, float radius, float rotationDeg = -90) {
  SkPathBuilder b;
  sides = std::max(sides, 3);
  const float rot = rotationDeg * (float)M_PI / 180.0f;
  for (int i = 0; i < sides; ++i) {
    const float a = rot + (float)i / (float)sides * 2.0f * (float)M_PI;
    const SkPoint p = {radius * std::cos(a), radius * std::sin(a)};
    i == 0 ? (void)b.moveTo(p) : (void)b.lineTo(p);
  }
  b.close();
  return b.detach();
}

inline SkPath star(int points, float radius, float innerRatio = 0.45f,
                   float rotationDeg = -90) {
  SkPathBuilder b;
  points = std::max(points, 3);
  const float rot = rotationDeg * (float)M_PI / 180.0f;
  for (int i = 0; i < points * 2; ++i) {
    const float r = i % 2 == 0 ? radius : radius * innerRatio;
    const float a = rot + (float)i / (float)(points * 2) * 2.0f *
                        (float)M_PI;
    const SkPoint p = {r * std::cos(a), r * std::sin(a)};
    i == 0 ? (void)b.moveTo(p) : (void)b.lineTo(p);
  }
  b.close();
  return b.detach();
}

inline SkPath pill(float width, float height) {
  const float r = std::min(width, height) * 0.5f;
  return SkPath::RRect(
      SkRRect::MakeRectXY(SkRect::MakeXYWH(-width / 2, -height / 2, width,
                                           height),
                          r, r));
}

inline SkPath ring(float outer, float inner) {
  SkPathBuilder b;
  b.addCircle(0, 0, outer);
  b.addCircle(0, 0, inner, SkPathDirection::kCCW);
  return b.detach();
}

// ---------------------------------------------------------------------------
// Shape — a base path + a stack of dials + one look.

class Shape {
public:
  explicit Shape(SkPath base) : m_base(std::move(base)) {}

  Shape &offset(float px) { return step(ops::offsetBy(px)); }
  Shape &roughen(float amp, uint32_t seed = 1) {
    return step(ops::Roughen{amp, 8, seed});
  }
  Shape &zigzag(float amp, float wavelength = 24, bool smooth = false) {
    return step(ops::Zigzag{amp, wavelength, smooth});
  }
  Shape &bloat(float amount) { return step(ops::PuckerBloat{amount}); }
  Shape &pucker(float amount) { return step(ops::PuckerBloat{-amount}); }
  Shape &twirl(float degrees) { return step(ops::Twirl{degrees}); }
  Shape &unite(SkPath other) {
    return step([other = std::move(other)](const SkPath &p) {
      return ops::unite(p, other);
    });
  }
  Shape &cut(SkPath other) {
    return step([other = std::move(other)](const SkPath &p) {
      return ops::subtract(p, other);
    });
  }
  Shape &clip(SkPath other) {
    return step([other = std::move(other)](const SkPath &p) {
      return ops::intersect(p, other);
    });
  }
  /** Any hand-rolled step. */
  Shape &step(ops::PathOp op) {
    m_steps.push_back(std::move(op));
    return *this;
  }

  // one look, last call wins
  Shape &fill(SkColor4f color) {
    m_fill = color;
    return *this;
  }
  Shape &stroke(SkColor4f color, float width = 3) {
    m_stroke = color;
    m_strokeWidth = width;
    return *this;
  }
  Shape &gold(const materials::Environment &env, float bevel = 8) {
    m_look = Look::Gold;
    m_env = &env;
    m_bevel = bevel;
    return *this;
  }
  Shape &chrome(const materials::Environment &env, float bevel = 10) {
    m_look = Look::Chrome;
    m_env = &env;
    m_bevel = bevel;
    return *this;
  }
  Shape &glass(const materials::Environment &env, sk_sp<SkImage> backdrop,
               float bevel = 12) {
    m_look = Look::Glass;
    m_env = &env;
    m_backdrop = std::move(backdrop);
    m_bevel = bevel;
    return *this;
  }

  /** The cooked outline (recipe applied, base untouched). */
  SkPath path() const {
    SkPath current = m_base;
    for (const ops::PathOp &op : m_steps)
      current = op(current);
    return current;
  }

  void draw(SkCanvas &canvas, SkPoint at = {0, 0}) const {
    canvas.save();
    canvas.translate(at.fX, at.fY);
    const SkPath cooked = path();
    switch (m_look) {
    case Look::Gold:
      if (m_env)
        materials::drawGold(canvas, cooked, *m_env, m_bevel);
      break;
    case Look::Chrome:
      if (m_env)
        materials::drawChrome(canvas, cooked, *m_env, m_bevel);
      break;
    case Look::Glass:
      if (m_env && m_backdrop)
        materials::drawGlass(canvas, cooked, *m_env, m_backdrop, m_bevel);
      break;
    case Look::Paint:
      break;
    }
    // Artist semantics: a bare chain gets the default fill; asking for
    // ONLY a stroke means stroke only; fill() always fills.
    const bool fills =
        m_look == Look::Paint && (m_fill || !m_stroke);
    if (fills) {
      SkPaint paint;
      paint.setAntiAlias(true);
      paint.setColor4f(m_fill.value_or(SkColor4f{0.9f, 0.9f, 0.95f, 1}));
      canvas.drawPath(cooked, paint);
    }
    if (m_stroke) {
      SkPaint paint;
      paint.setAntiAlias(true);
      paint.setStyle(SkPaint::kStroke_Style);
      paint.setStrokeWidth(m_strokeWidth);
      paint.setColor4f(*m_stroke);
      canvas.drawPath(cooked, paint);
    }
    canvas.restore();
  }

private:
  enum class Look : uint8_t { Paint, Gold, Chrome, Glass };

  SkPath m_base;
  std::vector<ops::PathOp> m_steps;
  Look m_look = Look::Paint;
  std::optional<SkColor4f> m_fill;
  std::optional<SkColor4f> m_stroke;
  float m_strokeWidth = 3;
  const materials::Environment *m_env = nullptr;
  sk_sp<SkImage> m_backdrop;
  float m_bevel = 8;
};

inline Shape shape(SkPath base) { return Shape(std::move(base)); }

// ---------------------------------------------------------------------------
// Blend — two outlines, a count, maybe a road to ride.

class Blend {
public:
  Blend(SkPath from, SkPath to) {
    m_from.path = std::move(from);
    m_to.path = std::move(to);
  }

  Blend &colors(SkColor4f from, SkColor4f to) {
    m_from.fill = from;
    m_to.fill = to;
    return *this;
  }
  Blend &steps(int count) {
    m_options.spacing = blend::Spacing::Steps;
    m_options.steps = count;
    return *this;
  }
  Blend &every(float px) {
    m_options.spacing = blend::Spacing::Distance;
    m_options.distance = px;
    return *this;
  }
  Blend &smoothColor() {
    m_options.spacing = blend::Spacing::SmoothColor;
    return *this;
  }
  /** Ride a spine; turning() rotates steps with its tangent. */
  Blend &along(SkPath spine) {
    m_options.spine = std::move(spine);
    return *this;
  }
  Blend &turning() {
    m_options.orientation = blend::Orientation::AlignToPath;
    return *this;
  }
  Blend &smooth() {
    m_options.smoothOutlines = true;
    return *this;
  }
  /** Place the endpoints (ignored when along() gave a spine). */
  Blend &between(SkPoint fromAt, SkPoint toAt) {
    m_fromAt = fromAt;
    m_toAt = toAt;
    return *this;
  }

  std::vector<blend::Step> cook() const {
    blend::Key from = m_from, to = m_to;
    from.path = m_from.path.makeTransform(
        SkMatrix::Translate(m_fromAt.fX, m_fromAt.fY));
    to.path = m_to.path.makeTransform(
        SkMatrix::Translate(m_toAt.fX, m_toAt.fY));
    return blend::make(from, to, m_options);
  }

  void draw(SkCanvas &canvas) const { blend::draw(canvas, cook()); }

private:
  blend::Key m_from, m_to;
  blend::Options m_options;
  SkPoint m_fromAt = {0, 0};
  SkPoint m_toAt = {0, 0};
};

inline Blend blend(SkPath from, SkPath to) {
  return Blend(std::move(from), std::move(to));
}

// ---------------------------------------------------------------------------
// Wire — a spline you can sweep, bead, or draw.

class Wire {
public:
  Wire() = default;
  Wire(std::initializer_list<SkV3> points) {
    m_spline.points = points;
  }
  explicit Wire(Spline3 spline) : m_spline(std::move(spline)) {}

  Wire &through(SkV3 point) {
    m_spline.points.push_back(point);
    return *this;
  }
  Wire &closed(bool value = true) {
    m_spline.closed = value;
    return *this;
  }
  Wire &straight() {
    m_spline.type = Spline3::Type::Linear;
    return *this;
  }

  const Spline3 &spline() const { return m_spline; }

  Mesh tube(float radius, int segments = 160) const {
    return curves::tube(m_spline,
                        {.radius = radius, .segments = segments});
  }
  Mesh ribbon(float width, int segments = 160) const {
    return curves::ribbon(m_spline,
                          {.width = width, .segments = segments});
  }
  Cloud beads(int count) const {
    return points::onSpline(m_spline, count);
  }

  /** Draw the wire itself as a stroked overlay. */
  void draw(SkCanvas &canvas, const space::Camera &camera,
            SkSize viewport, SkColor4f color = {1, 1, 1, 0.6f},
            float width = 2) const {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(width);
    paint.setColor4f(color);
    canvas.drawPath(curves::project(m_spline, camera, viewport, 256),
                    paint);
  }

private:
  Spline3 m_spline;
};

inline Wire wire(std::initializer_list<SkV3> points) {
  return Wire(points);
}

// ---------------------------------------------------------------------------
// Particles — put points somewhere, style them, glow.

class Particles {
public:
  Particles &on(const Wire &wire) {
    m_wire = wire;
    m_source = Source::Wire;
    return *this;
  }
  Particles &inBox(SkV3 lo, SkV3 hi) {
    m_lo = lo;
    m_hi = hi;
    m_source = Source::Box;
    return *this;
  }
  Particles &onSurface(const Mesh &mesh) {
    m_mesh = &mesh;
    m_source = Source::Mesh;
    return *this;
  }
  Particles &count(int value) {
    m_count = value;
    return *this;
  }
  Particles &seed(uint32_t value) {
    m_seed = value;
    return *this;
  }
  /** Organic value-noise displacement, px. */
  Particles &drift(float amount, float frequency = 0.012f) {
    m_drift = amount;
    m_driftFrequency = frequency;
    return *this;
  }
  Particles &jitter(float amount) {
    m_jitter = amount;
    return *this;
  }
  Particles &size(float base, float vary = 0) {
    m_size = base;
    m_sizeVary = vary;
    return *this;
  }
  /** Tint from a to b along the "t" lane. */
  Particles &ramp(SkColor4f a, SkColor4f b) {
    m_rampA = a;
    m_rampB = b;
    return *this;
  }
  Particles &sprite(sk_sp<SkImage> image) {
    m_sprite = std::move(image);
    return *this;
  }

  Cloud cook() const {
    Cloud cloud;
    switch (m_source) {
    case Source::Wire:
      cloud = points::onSpline(m_wire.spline(), m_count);
      break;
    case Source::Box:
      cloud = points::scatterBox(m_lo, m_hi, m_count, m_seed);
      break;
    case Source::Mesh:
      if (m_mesh)
        cloud = points::onMesh(*m_mesh, m_count, m_seed);
      break;
    }
    if (m_jitter > 0)
      points::jitter(cloud, m_jitter, m_seed + 1);
    if (m_drift > 0)
      points::displaceNoise(cloud, m_drift, m_driftFrequency, m_seed + 2);
    const std::vector<float> *t = cloud.scalarIf("t");
    std::vector<SkColor4f> &tint = cloud.color("tint");
    std::vector<float> &size = cloud.scalar("size", 1);
    for (size_t i = 0; i < cloud.size(); ++i) {
      const float f =
          t && i < t->size()
              ? (*t)[i]
              : (cloud.size() > 1
                     ? (float)i / (float)(cloud.size() - 1)
                     : 0.0f);
      tint[i] = {m_rampA.fR + (m_rampB.fR - m_rampA.fR) * f,
                 m_rampA.fG + (m_rampB.fG - m_rampA.fG) * f,
                 m_rampA.fB + (m_rampB.fB - m_rampA.fB) * f,
                 m_rampA.fA + (m_rampB.fA - m_rampA.fA) * f};
      size[i] = 1.0f + m_sizeVary * std::sin(f * 37.0f);
    }
    return cloud;
  }

  void glow(SkCanvas &canvas, const space::Camera &camera,
            SkSize viewport) const {
    points::BillboardStyle style;
    style.size = m_size;
    style.sizeLane = "size";
    style.tintLane = "tint";
    style.sprite = m_sprite;
    points::drawBillboards(canvas, cook(), camera, viewport, style);
  }

private:
  enum class Source : uint8_t { Wire, Box, Mesh };

  Source m_source = Source::Box;
  Wire m_wire;
  SkV3 m_lo = {-100, -100, -100}, m_hi = {100, 100, 100};
  const Mesh *m_mesh = nullptr;
  int m_count = 200;
  uint32_t m_seed = 7;
  float m_drift = 0, m_driftFrequency = 0.012f;
  float m_jitter = 0;
  float m_size = 10, m_sizeVary = 0.5f;
  SkColor4f m_rampA = {0.4f, 0.8f, 1.0f, 0.5f};
  SkColor4f m_rampB = {1.0f, 0.5f, 0.9f, 0.5f};
  sk_sp<SkImage> m_sprite;
};

inline Particles particles() { return Particles(); }

} // namespace sigil::shape::easel
