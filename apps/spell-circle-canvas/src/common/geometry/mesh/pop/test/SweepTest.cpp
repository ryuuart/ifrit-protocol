/** @file
 * The swept operator: the profiles it carries and the shapes they form —
 * a round profile a tube, a flat one a ribbon or a hung banner, each
 * also written out longhand at the foot of this file and held against
 * the sweep vertex for vertex — and its ring seam: what a rail and a
 * profile become as a dispatch, that the value carrying an executor
 * compares like the model it holds, and that a substituted executor is
 * the one that forms the vertices.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/curve/Curve.h"
#include "sigilgeometry/mesh/pop/Sweep.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

namespace {

using curve::Frame3;
using pop::SweepOptions;
using pop::SweepRuntime;

/** A short straight rail: two frames a unit apart, axis-aligned. */
std::vector<Frame3> rail(size_t count = 4) {
  std::vector<Frame3> out;
  for (size_t i = 0; i < count; ++i) {
    Frame3 f;
    f.position = {0, 0, (float)i};
    f.tangent = {0, 0, 1};
    f.normal = {0, 1, 0};
    f.binormal = {1, 0, 0};
    f.t = count > 1 ? (float)i / (float)(count - 1) : 0.0f;
    out.push_back(f);
  }
  return out;
}

/** An executor that counts what it was handed and then answers exactly
 *  what the built-in one would. */
struct CountingExecutor : pop::SweepExecutor {
  std::shared_ptr<int> calls = std::make_shared<int>(0);

  bool operator==(const CountingExecutor& other) const {
    return calls == other.calls;
  }
  std::string name() const override { return "counting"; }
  void rings(const pop::kernel::Dispatch& work, glm::vec4* positions,
             glm::vec4* normals) const override {
    ++*calls;
    pop::kernel::run(work, positions, normals);
  }
};

TEST(Sweep, RuntimeIsOneValue) {
  EXPECT_EQ(SweepRuntime::cpu(), SweepRuntime::cpu());
  EXPECT_EQ(SweepOptions{}.runtime, SweepRuntime::cpu());
  const SweepRuntime counting{CountingExecutor{}};
  EXPECT_NE(counting, SweepRuntime::cpu());
  EXPECT_EQ(counting, SweepRuntime(counting));
  // Two separately made models hold separate state and are not one
  // runtime, which is what a reconciler asking "did the runtime change"
  // has to be told.
  EXPECT_NE(counting, SweepRuntime{CountingExecutor{}});
}

TEST(Sweep, DescribePacksTheRailAndTheProfile) {
  const std::vector<Frame3> r = rail(5);
  const sigil::geometry::path::Polyline p = pop::profile::circle(8);
  SweepOptions options;
  options.scale = 2.0f;
  options.taper = [](float t) { return 1.0f + t; };

  pop::kernel::Dispatch work;
  ASSERT_TRUE(pop::describe(r, p, options, &work));
  EXPECT_EQ(work.args.code.x, 5u);
  EXPECT_EQ(work.args.code.y, p.points.size());
  EXPECT_EQ(work.args.code.z, 1u) << "Radial is the default";
  EXPECT_EQ(work.args.code.w, p.points.size() - 1) << "an open profile";
  EXPECT_EQ(work.vertices(), 5u * p.points.size());
  ASSERT_EQ(work.railPosition.size(), 5u);
  // The taper is evaluated on the host, once per ring, and arrives as
  // the size that ring scales by.
  EXPECT_FLOAT_EQ(work.railPosition.front().w, 2.0f);
  EXPECT_FLOAT_EQ(work.railPosition.back().w, 4.0f);
  EXPECT_FLOAT_EQ(work.railNormal.back().w, 1.0f) << "the frame's t";

  // Nothing to sweep is said by the answer, not by an empty mesh.
  pop::kernel::Dispatch none;
  EXPECT_FALSE(pop::describe(rail(1), p, options, &none));
  EXPECT_FALSE(
      pop::describe(r, sigil::geometry::path::Polyline{}, options, &none));
}

TEST(Sweep, ASubstitutedExecutorFormsTheVertices) {
  const std::vector<Frame3> r = rail(6);
  const sigil::geometry::path::Polyline p = pop::profile::circle(10);

  const Mesh built = pop::sweep(r, p);
  SweepOptions options;
  const CountingExecutor counting;
  options.runtime = SweepRuntime{counting};
  const Mesh substituted = pop::sweep(r, p, options);

  EXPECT_EQ(*counting.calls, 1) << "one dispatch forms every ring";
  ASSERT_EQ(built.positions.size(), substituted.positions.size());
  EXPECT_EQ(built.positions, substituted.positions);
  EXPECT_EQ(built.normals, substituted.normals);
  EXPECT_EQ(built.uvs, substituted.uvs);
  EXPECT_EQ(built.indices, substituted.indices);
}

TEST(Sweep, EveryNormalRuleIsFormedFromTheSameRings) {
  const std::vector<Frame3> r = rail(6);
  const sigil::geometry::path::Polyline p = pop::profile::circle(10);
  for (SweepOptions::Normals rule :
       {SweepOptions::Normals::Radial, SweepOptions::Normals::Frame,
        SweepOptions::Normals::Geometric}) {
    SweepOptions options;
    options.normals = rule;
    const Mesh mesh = pop::sweep(r, p, options);
    EXPECT_EQ(mesh.positions.size(), r.size() * p.points.size());
    EXPECT_EQ(mesh.normals.size(), mesh.positions.size());
    // The positions do not depend on where a normal came from.
    SweepOptions radial;
    EXPECT_EQ(mesh.positions, pop::sweep(r, p, radial).positions);
  }
}

}  // namespace

TEST(Curves, SweptProfilesAreWellFormed) {
  curve::Spline3 arc;
  arc.points = {{0, 0, 0}, {60, 60, 0}, {120, 0, 0}};
  const Mesh t = pop::sweep(arc, pop::profile::circle(8),
                            {.segments = 24, .scale = 8, .caps = true});
  EXPECT_GT(t.triangleCount(), 0u);
  EXPECT_EQ(t.normals.size(), t.vertexCount());
  for (const glm::vec3& n : t.normals) EXPECT_NEAR(glm::length(n), 1, 1e-3);
  // A line profile sweeps to a strip: `segments` cross-sections of two
  // vertices each, and two triangles per gap between consecutive sections.
  const Mesh r = pop::sweep(arc, pop::profile::line(),
                            {.segments = 24,
                             .scale = 20,
                             .normals = pop::SweepOptions::Normals::Frame});
  EXPECT_EQ(r.vertexCount(), 48u);    // 24 * 2
  EXPECT_EQ(r.triangleCount(), 46u);  // (24 - 1) * 2
}

// A profile is a Polyline, so anything that flattens to one sweeps: the
// door the 2D shape vocabulary comes through. A closed outline wraps
// back onto its first point and forms one more quad per ring than an
// open one of the same point count.
TEST(Curves, SweepCarriesAnyFlattenedOutline) {
  curve::Spline3 arc;
  arc.type = curve::Spline3::Type::Linear;
  arc.points = {{0, 0, 0}, {0, 0, 200}};
  SkPathBuilder square;
  square.moveTo(-10, -10).lineTo(10, -10).lineTo(10, 10).lineTo(-10, 10);
  square.close();
  const path::Polyline outline = pop::profile::fromPath(square.detach());
  EXPECT_TRUE(outline.closed);
  const Mesh box = pop::sweep(
      arc, outline,
      {.segments = 8, .normals = pop::SweepOptions::Normals::Geometric});
  EXPECT_EQ(box.vertexCount(), 8u * (uint32_t)outline.points.size());
  EXPECT_EQ(box.normals.size(), box.vertexCount());
  glm::vec3 lo, hi;
  box.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 20, 1e-3);
  EXPECT_NEAR(hi.z - lo.z, 200, 1e-3);
}

// `scale` sizes the profile and `taper` reshapes it along the curve.
// A profile is a UNIT shape precisely so these two dials, not the
// profile's own construction, decide how big the sweep is.
TEST(Curves, ScaleAndTaperSizeTheProfile) {
  curve::Spline3 arc;
  arc.type = curve::Spline3::Type::Linear;
  arc.points = {{0, 0, 0}, {0, 0, 200}};
  const Mesh even =
      pop::sweep(arc, pop::profile::circle(16), {.segments = 32, .scale = 10});
  glm::vec3 lo, hi;
  even.bounds(&lo, &hi);
  EXPECT_NEAR(hi.x - lo.x, 20, 0.2f);

  const Mesh cone = pop::sweep(
      arc, pop::profile::circle(16),
      {.segments = 32, .scale = 10, .taper = [](float t) { return t; }});
  // The first ring collapses onto the curve and the last is full width.
  EXPECT_NEAR(glm::length(cone.positions[0] - arc.position(0)), 0, 1e-3);
  EXPECT_NEAR(glm::length(cone.positions.back() - arc.position(1)), 10, 0.2f);
}

TEST(Curves, HungRailKeepsAProfileUpright) {
  curve::Spline3 loop;
  loop.closed = true;
  for (int i = 0; i < 12; ++i) {
    const float a = (float)i / 12.0f * 2.0f * (float)M_PI;
    loop.points.emplace_back(300.0f * std::cos(a), 40.0f * std::sin(2 * a),
                             300.0f * std::sin(a));
  }
  const Mesh band =
      pop::sweep(curve::hangFrames(loop, 120), pop::profile::line(),
                 {.scale = 50, .normals = pop::SweepOptions::Normals::Frame});
  ASSERT_EQ(band.vertexCount(), 240u);
  // A hung rail hangs: its across-vector is held vertical in world space
  // rather than rolling with the curve's frame, which keeps text upright
  // all the way round a closed loop. Vertices come in pairs per section, so
  // the first of every pair must sit ABOVE its partner in y everywhere.
  for (size_t i = 0; i + 1 < band.positions.size(); i += 2)
    EXPECT_GT(band.positions[i].y, band.positions[i + 1].y);
}

// The three shapes written longhand: a tube, a ribbon and a banner,
// each formed by its own dedicated body rather than by a profile on a
// rail. Holding the sweep against these is what makes "the same shape"
// mean the same floats and not merely the same picture — a rendered
// plate is compared byte for byte downstream, so a rounding step that
// moved would show up there as a changed image.
namespace reference {

Mesh tube(const curve::Spline3& spline, float radius,
          const std::function<float(float)>& taper, int segments, int sides,
          bool caps, glm::vec3 up) {
  Mesh out;
  segments = std::max(segments, 2);
  sides = std::max(sides, 3);
  const std::vector<curve::Frame3> rail = curve::frames(spline, segments, up);

  for (int i = 0; i < segments; ++i) {
    const curve::Frame3& f = rail[(size_t)i];
    const float r = radius * (taper ? std::max(taper(f.t), 0.0f) : 1.0f);
    for (int s = 0; s <= sides; ++s) {  // seam duplicated for clean UVs
      const float a = (float)s / (float)sides * 2.0f * (float)M_PI;
      const glm::vec3 dir = f.normal * std::cos(a) + f.binormal * std::sin(a);
      out.positions.push_back(f.position + dir * r);
      out.normals.push_back(dir);
      out.uvs.emplace_back((float)s / (float)sides, f.t);
    }
  }
  const int ring = sides + 1;
  for (int i = 0; i + 1 < segments; ++i)
    for (int s = 0; s < sides; ++s) {
      const uint32_t a = (uint32_t)(i * ring + s);
      const uint32_t b = a + 1;
      const uint32_t c = a + (uint32_t)ring;
      const uint32_t d = c + 1;
      out.indices.insert(out.indices.end(), {a, b, d, a, d, c});
    }

  if (caps && !spline.closed) {
    for (int end = 0; end < 2; ++end) {
      const curve::Frame3& f = rail[end == 0 ? 0 : rail.size() - 1];
      const glm::vec3 n = end == 0 ? f.tangent * -1.0f : f.tangent;
      const uint32_t center = (uint32_t)out.positions.size();
      out.positions.push_back(f.position);
      out.normals.push_back(n);
      out.uvs.emplace_back(0.5f, end == 0 ? 0.0f : 1.0f);
      const uint32_t ringStart =
          (uint32_t)((end == 0 ? 0 : segments - 1) * ring);
      for (int s = 0; s < sides; ++s) {
        const uint32_t a = ringStart + (uint32_t)s;
        const uint32_t b = ringStart + (uint32_t)s + 1;
        if (end == 0)
          out.indices.insert(out.indices.end(), {center, b, a});
        else
          out.indices.insert(out.indices.end(), {center, a, b});
      }
    }
  }
  return out;
}

Mesh ribbon(const curve::Spline3& spline, float width,
            const std::function<float(float)>& taper, int segments,
            glm::vec3 up) {
  Mesh out;
  segments = std::max(segments, 2);
  const std::vector<curve::Frame3> rail = curve::frames(spline, segments, up);
  for (int i = 0; i < segments; ++i) {
    const curve::Frame3& f = rail[(size_t)i];
    const float half =
        width * 0.5f * (taper ? std::max(taper(f.t), 0.0f) : 1.0f);
    out.positions.push_back(f.position - f.binormal * half);
    out.positions.push_back(f.position + f.binormal * half);
    out.normals.push_back(f.normal);
    out.normals.push_back(f.normal);
    out.uvs.emplace_back(0, f.t);
    out.uvs.emplace_back(1, f.t);
  }
  for (int i = 0; i + 1 < segments; ++i) {
    const uint32_t a = (uint32_t)(i * 2);
    out.indices.insert(out.indices.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
  }
  return out;
}

Mesh banner(const curve::Spline3& spline, float width, float head, float span,
            int sections) {
  Mesh out;
  sections = std::max(sections, 2);
  const float half = width * 0.5f;
  const auto wrap01 = [](float t) { return t - std::floor(t); };
  glm::vec3 hang = {0, -1, 0};  // carried through vertical stretches
  for (int i = 0; i < sections; ++i) {
    const float f = (float)i / (float)(sections - 1);
    const float t = wrap01(head - span + span * f);
    const glm::vec3 p = spline.position(t);
    glm::vec3 tangent = spline.position(wrap01(t + 0.002f)) -
                        spline.position(wrap01(t - 0.002f));
    const float len = glm::length(tangent);
    if (len > 1e-6f) tangent = tangent * (1.0f / len);
    glm::vec3 down =
        glm::vec3{0, -1, 0} - tangent * glm::dot(tangent, glm::vec3{0, -1, 0});
    const float downLen = glm::length(down);
    if (downLen > 0.15f) hang = down * (1.0f / downLen);
    const glm::vec3 normal = glm::cross(hang, tangent);
    out.positions.push_back(p - hang * half);  // u = 0: top edge
    out.positions.push_back(p + hang * half);
    out.normals.push_back(normal);
    out.normals.push_back(normal);
    out.uvs.emplace_back(0, f);
    out.uvs.emplace_back(1, f);
  }
  for (int i = 0; i + 1 < sections; ++i) {
    const uint32_t a = (uint32_t)(i * 2);
    out.indices.insert(out.indices.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
  }
  return out;
}

/** Every lane, value for value. */
void expectSame(const Mesh& made, const Mesh& want) {
  ASSERT_EQ(made.positions.size(), want.positions.size());
  ASSERT_EQ(made.normals.size(), want.normals.size());
  ASSERT_EQ(made.uvs.size(), want.uvs.size());
  ASSERT_EQ(made.indices, want.indices);
  for (size_t i = 0; i < want.positions.size(); ++i) {
    EXPECT_EQ(made.positions[i], want.positions[i]) << "position " << i;
    EXPECT_EQ(made.normals[i], want.normals[i]) << "normal " << i;
    EXPECT_EQ(made.uvs[i], want.uvs[i]) << "uv " << i;
  }
}

curve::Spline3 knot() {
  curve::Spline3 spline;
  spline.closed = true;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    spline.points.emplace_back(std::cos(a) * 300, std::sin(a * 3.0f) * 110,
                               std::sin(a) * 300);
  }
  return spline;
}

curve::Spline3 arc() {
  curve::Spline3 spline;
  spline.points = {
      {-820, 260, -320}, {-300, 420, 60}, {260, 300, 220}, {820, 430, -260}};
  return spline;
}

}  // namespace reference

// A circle profile IS the tube: the ring the old generator evaluated
// around each frame, emitted once as a unit contour with its seam point
// duplicated, then scaled on the frame. Both the open case (which grows
// end caps) and the closed one (which cannot) have to land on the same
// floats.
TEST(Curves, CircleProfileReproducesTheTube) {
  reference::expectSame(
      pop::sweep(reference::knot(), pop::profile::circle(12),
                 {.segments = 220, .scale = 9}),
      reference::tube(reference::knot(), 9, nullptr, 220, 12, true, {0, 1, 0}));
  reference::expectSame(
      pop::sweep(reference::arc(), pop::profile::circle(10),
                 {.segments = 180, .scale = 7, .caps = true}),
      reference::tube(reference::arc(), 7, nullptr, 180, 10, true, {0, 1, 0}));
  // The taper multiplies the radius exactly where the old profile
  // function did — before the offset leaves the frame, not after.
  const std::function<float(float)> pinch = [](float t) {
    return 0.2f + std::sin(t * 3.0f);
  };
  reference::expectSame(
      pop::sweep(reference::arc(), pop::profile::circle(6),
                 {.segments = 40,
                  .scale = 12,
                  .taper = pinch,
                  .up = {0, 0, 1},
                  .caps = true}),
      reference::tube(reference::arc(), 12, pinch, 40, 6, true, {0, 0, 1}));
}

// A two-point line profile IS the ribbon, and the rail's own normal is
// its facing — a flat band has one, where a round profile's outward is
// the offset itself.
TEST(Curves, LineProfileReproducesTheRibbon) {
  const std::function<float(float)> swell = [](float t) {
    return 1.0f - 0.6f * t;
  };
  for (const std::function<float(float)>& taper :
       {std::function<float(float)>{}, swell}) {
    reference::expectSame(
        pop::sweep(reference::knot(), pop::profile::line(),
                   {.segments = 220,
                    .scale = 30,
                    .taper = taper,
                    .normals = pop::SweepOptions::Normals::Frame}),
        reference::ribbon(reference::knot(), 30, taper, 220, {0, 1, 0}));
    reference::expectSame(
        pop::sweep(reference::arc(), pop::profile::line(),
                   {.segments = 96,
                    .scale = 42,
                    .taper = taper,
                    .up = {1, 0, 0},
                    .normals = pop::SweepOptions::Normals::Frame}),
        reference::ribbon(reference::arc(), 42, taper, 96, {1, 0, 0}));
  }
}

// The banner was the same line profile on a different RAIL: a window of
// a closed loop walked in parameter with its across-vector held world
// vertical. Splitting the rail from the sweep is what let the third
// generator go without changing a vertex.
TEST(Curves, LineProfileOnAHungRailReproducesTheBanner) {
  const curve::Spline3 loop = reference::knot();
  for (int k = 0; k < 4; ++k) {
    const float head = (float)(k + 1) / 4.0f;
    reference::expectSame(
        pop::sweep(curve::hangFrames(loop, 160, head, 0.25f),
                   pop::profile::line(),
                   {.scale = 64, .normals = pop::SweepOptions::Normals::Frame}),
        reference::banner(loop, 64, head, 0.25f, 160));
  }
}
