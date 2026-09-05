/** @file
 * The sinks a chain reaches by its own verb: the swept ones, which resample
 * the cooked points into a rail and carry a profile along it; the stamping
 * one, which instances a mesh at every point and remaps its uvs into the
 * atlas cell the point was dealt; and the splatting one, which draws the
 * cooked points onto a canvas.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/curve/Curve.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "sigilgeometry/path/Polyline.h"
#include "support/Loops.h"
#include <sigilgeometry/kit/Sections.h>

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;
using sigil::geometry::mesh::pop::test::flatRing;


TEST(Pop, SweptSinksBendWithTheChain) {
  // The chain's cooked POINTS are the path a sweep follows, so any operator
  // that moves points also bends every swept surface built from the chain.
  // The same description feeds a round profile and a flat one unchanged.
  pop::SplineScatter scatter;
  scatter.loop = flatRing(10, 300.0);
  scatter.count = 96;
  scatter.head = 1;
  scatter.span = 1;
  pop::Chain chain = {scatter, pop::Noise{pop::Lane::P, 40, 0.01f, 5}};

  const Mesh tube = pop::cookSweep(chain, sections::circle(10), true,
                                   {.segments = 200, .scale = 12});
  EXPECT_GT(tube.triangleCount(), 1000u);
  glm::vec3 lo, hi;
  tube.bounds(&lo, &hi);
  EXPECT_GT(hi.y - lo.y, 20.0f) << "noise must bend the sweep off-plane";
  // 600 across the scattered circle, plus the tube radius on each side;
  // the wide tolerance is the noise, which is free to push either way.
  EXPECT_NEAR(hi.x - lo.x, 624, 130);

  const Mesh ribbon =
      pop::cookSweep(chain, sections::line(), true,
                     {.segments = 160,
                      .scale = 60,
                      .normals = pop::SweepOptions::Normals::Frame});
  EXPECT_GT(ribbon.triangleCount(), 200u);

  chain.emplace_back(pop::Math{pop::Lane::P, {1, 1, 1, 1}, {0, 900, 0, 0}});
  const Mesh lifted = pop::cookSweep(chain, sections::circle(10), true,
                                     {.segments = 160, .scale = 12});
  glm::vec3 lo2, hi2;
  lifted.bounds(&lo2, &hi2);
  EXPECT_GT(lo2.y, hi.y + 400.0f) << "value edit re-forms the model high";
}

TEST(Pop, SweepCarriesAnyProfileAlongTheChain) {
  const std::vector<glm::vec3> loop = flatRing(8, 220.0);
  SkPathBuilder starProfile;
  for (int i = 0; i < 10; ++i) {
    const float r = i % 2 == 0 ? 24.0f : 10.0f;
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    const SkPoint p = {r * std::cos(a), r * std::sin(a)};
    if (i == 0)
      starProfile.moveTo(p);
    else
      starProfile.lineTo(p);
  }
  starProfile.close();
  const Mesh swept = pop::on(loop).count(60).smooth(0.4f).sweep(
      pop::profile::fromPath(starProfile.detach()), true,
      {.segments = 120, .normals = pop::SweepOptions::Normals::Geometric});
  EXPECT_GT(swept.triangleCount(), 1500u);
  glm::vec3 lo, hi;
  swept.bounds(&lo, &hi);
  // The swept profile is carried in the frame's normal plane, so the star's
  // longest arm adds its radius on each side of the 220 scatter circle...
  EXPECT_NEAR(hi.x - lo.x, 2 * (220 + 24), 30);
  // ...and stands out of the loop's own plane rather than lying flat in it.
  EXPECT_GT(hi.y - lo.y, 20.0f);
}

TEST(Pop, AtlasTexHintsRemapStampUvs) {
  const std::vector<glm::vec3> loop = flatRing(8, 150.0);
  const pop::Chain chain = pop::on(loop).count(40).atlas(2, 2);
  const Mesh stamped = pop::cookMesh(chain, mesh::quad(8, 8));
  // atlas(2, 2) divides the texture into a 2x2 grid and assigns each point
  // one cell, remapping its stamp's uvs into that cell. So every stamp's
  // uvs span exactly half the range in each axis and sit wholly inside one
  // cell — a stamp straddling a cell edge would sample two sprites at once.
  const size_t stampVerts = mesh::quad(8, 8).vertexCount();
  int cellsSeen[4] = {0, 0, 0, 0};
  for (size_t p = 0; p < 40; ++p) {
    float uMin = 2, uMax = -1, vMin = 2, vMax = -1;
    for (size_t v = 0; v < stampVerts; ++v) {
      const glm::vec2 uv = stamped.uvs[p * stampVerts + v];
      uMin = std::min(uMin, uv.x);
      uMax = std::max(uMax, uv.x);
      vMin = std::min(vMin, uv.y);
      vMax = std::max(vMax, uv.y);
    }
    EXPECT_NEAR(uMax - uMin, 0.5f, 1e-4f);
    EXPECT_NEAR(vMax - vMin, 0.5f, 1e-4f);
    cellsSeen[(uMin > 0.25f ? 1 : 0) + (vMin > 0.25f ? 2 : 0)]++;
  }
  // Every one of the 40 points fell into one of the four cells.
  EXPECT_GT(cellsSeen[0] + cellsSeen[1] + cellsSeen[2] + cellsSeen[3], 39);
  int distinct = 0;
  for (int c : cellsSeen) distinct += c > 0 ? 1 : 0;
  EXPECT_GE(distinct, 3) << "the hash should spread across cells";
}

// DELETE is the other half of SELECT: the selector names a region and
// this removes what it named. Only the count moves — every lane that
// survives is the value its own point carried, so a chain that deletes
// and then reads a lane reads the right point's value and not its
// neighbour's.
// THE SPLATTING SINK is a member of the family like the forming ones: a
// chain reaches it by its own verb, and the lanes the cook exports are
// the ones the splat reads without the caller naming them again.
TEST(Pop, TheBillboardSinkSplatsAChainsCookedPoints) {
  const sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(160, 120));
  ASSERT_TRUE(surface);
  surface->getCanvas()->clear(SK_ColorBLACK);
  camera::Camera camera;
  camera.eye = {0, 0, 420};

  pop::on(flatRing(12, 90))
      .count(200)
      .vary(0.5f, 1.4f)
      .tint({1, 0.6f, 0.2f, 1})
      .billboards(*surface->getCanvas(), camera, {160, 120}, {.size = 6});

  SkBitmap shot;
  shot.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  ASSERT_TRUE(surface->makeImageSnapshot()->readPixels(shot.pixmap(), 0, 0));
  int lit = 0;
  for (int y = 0; y < shot.height(); ++y)
    for (int x = 0; x < shot.width(); ++x)
      if (shot.getColor(x, y) != SK_ColorBLACK) ++lit;
  EXPECT_GT(lit, 200);
}

// The swept sink cooks the chain's points into a Catmull-Rom path and
// hands that, with the profile, to pop::sweep. Below it is written
// longhand — the profile flattened off an SkPath, wrapped back onto its
// first point, rings formed in place and normals averaged from the
// triangles — so the forwarding can be held against it. The topology
// and every uv agree exactly. The positions agree to within one
// rounding step, and the direction of that step is deliberate: the
// primitive assembles the profile's offset before adding the frame's
// position, where the longhand adds the position first and so spends a
// bit of the offset's precision on a spine far from the origin.
namespace {

Mesh referenceSweep(const pop::Chain& chain, const SkPath& profile, bool closed,
                    int segments) {
  curve::Spline3 spine;
  spine.points = pop::cook(chain).positions;
  spine.closed = closed;
  if (spine.points.size() < 2) return {};
  const std::vector<path::Polyline> contours = path::flatten(profile, 0.4f);
  if (contours.empty() || contours[0].points.size() < 3) return {};
  const std::vector<glm::vec2>& ring = contours[0].points;
  const std::vector<curve::Frame3> rail =
      curve::frames(spine, std::max(segments, 2), {0, 1, 0});

  Mesh out;
  const uint32_t n = (uint32_t)ring.size();
  for (const curve::Frame3& f : rail)
    for (uint32_t i = 0; i < n; ++i) {
      const glm::vec2 p = ring[i];
      out.positions.push_back(f.position + f.binormal * p.x - f.normal * p.y);
      out.uvs.emplace_back((float)i / (float)n, f.t);
    }
  for (uint32_t s = 0; s + 1 < (uint32_t)rail.size(); ++s)
    for (uint32_t i = 0; i < n; ++i) {
      const uint32_t j = (i + 1) % n;
      const uint32_t a = s * n + i, b = s * n + j;
      const uint32_t c = (s + 1) * n + i, d = (s + 1) * n + j;
      out.indices.insert(out.indices.end(), {a, b, d, a, d, c});
    }
  out.computeNormals();
  return out;
}

SkPath starProfile() {
  SkPathBuilder b;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 2.0f * (float)M_PI;
    const float r = (i % 2 == 0) ? 24.0f : 10.0f;
    const SkPoint p = {std::cos(a) * r, std::sin(a) * r};
    if (i == 0)
      b.moveTo(p);
    else
      b.lineTo(p);
  }
  b.close();
  return b.detach();
}

}  // namespace

TEST(Pop, SweptSinkForwardsToTheSweptPrimitive) {
  const std::vector<glm::vec3> loop = flatRing(8, 220.0f);
  const pop::Chain chain = pop::on(loop).count(60).noise(18).smooth(0.4f);

  for (const bool closed : {false, true}) {
    const Mesh made = pop::cookSweep(
        chain, pop::profile::fromPath(starProfile()), closed,
        {.segments = 120, .normals = pop::SweepOptions::Normals::Geometric});
    const Mesh want = referenceSweep(chain, starProfile(), closed, 120);
    ASSERT_EQ(made.positions.size(), want.positions.size());
    ASSERT_EQ(made.indices, want.indices);
    for (size_t i = 0; i < want.positions.size(); ++i) {
      // A float carrying a coordinate of a few hundred steps by about
      // 3e-5; the bound is two of those, and the normals are unit.
      EXPECT_LT(glm::length(made.positions[i] - want.positions[i]), 1e-4f)
          << "position " << i;
      EXPECT_LT(glm::length(made.normals[i] - want.normals[i]), 1e-4f)
          << "normal " << i;
      EXPECT_EQ(made.uvs[i], want.uvs[i]) << "uv " << i;
    }
  }

  // The sink is the chain's only geometric commitment: the same chain,
  // a different profile, and the model changes without the description
  // being touched.
  const Mesh cable = pop::cookSweep(chain, sections::circle(10), true,
                                    {.segments = 120, .scale = 9});
  EXPECT_EQ(cable.vertexCount(), 120u * 11u);
  EXPECT_EQ(cable.normals.size(), cable.vertexCount());
  // A chain too short to be a path forms nothing rather than a
  // degenerate mesh.
  EXPECT_TRUE(pop::cookSweep(pop::Chain{}, sections::circle(), false)
                  .positions.empty());
}
