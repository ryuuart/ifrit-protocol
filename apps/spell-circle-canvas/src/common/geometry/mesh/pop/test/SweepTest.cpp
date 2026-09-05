/** @file
 * The swept operator's ring seam: what a rail and a profile become as a
 * dispatch, that the taper is resolved on the host once per ring, that the
 * value carrying an executor compares like the model it holds, and that a
 * substituted executor is the one that forms the vertices.
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/curve/Curve.h"
#include "sigilgeometry/mesh/pop/Sweep.h"
#include "support/RuntimeSeam.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

namespace {

using curve::Frame3;
using pop::SweepOptions;
using pop::SweepRuntime;

/** A short straight rail: frames a unit apart, axis-aligned. */
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
 *  what the built-in one would. Two of them compare by the label they
 *  carry, which is what makes a runtime holding one a comparable value. */
struct CountingExecutor : pop::SweepExecutor {
  explicit CountingExecutor(std::string called) : label(std::move(called)) {}

  std::string label;
  std::shared_ptr<int> calls = std::make_shared<int>(0);

  bool operator==(const CountingExecutor& other) const {
    return label == other.label;
  }
  std::string name() const override { return label; }
  void rings(const pop::kernel::Dispatch& work, glm::vec4* positions,
             glm::vec4* normals) const override {
    ++*calls;
    pop::kernel::run(work, positions, normals);
  }
};

struct SweepSeam {
  using Seam = SweepRuntime;
  static Seam builtIn() { return SweepRuntime::cpu(); }
  static Seam holding(const char* label) {
    return SweepRuntime{CountingExecutor{label}};
  }
};

}  // namespace

INSTANTIATE_TYPED_TEST_SUITE_P(TheSweepsRingSeam, RuntimeSeam, SweepSeam);

namespace {

TEST(MeshSweep, TheDefaultOptionsCarryTheBuiltInRuntime) {
  EXPECT_EQ(SweepOptions{}.runtime, SweepRuntime::cpu());
}

TEST(MeshSweep, DescribePacksTheRailAndTheProfile) {
  const std::vector<Frame3> r = rail(5);
  const path::Polyline p = pop::profile::circle(8);
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
  EXPECT_FALSE(pop::describe(r, path::Polyline{}, options, &none));
}

TEST(MeshSweep, ASubstitutedExecutorFormsTheVertices) {
  const std::vector<Frame3> r = rail(6);
  const path::Polyline p = pop::profile::circle(10);

  const Mesh built = pop::sweep(r, p);
  SweepOptions options;
  const CountingExecutor counting{"counting"};
  options.runtime = SweepRuntime{counting};
  const Mesh substituted = pop::sweep(r, p, options);

  EXPECT_EQ(*counting.calls, 1) << "one dispatch forms every ring";
  ASSERT_EQ(built.positions.size(), substituted.positions.size());
  EXPECT_EQ(built.positions, substituted.positions);
  EXPECT_EQ(built.normals, substituted.normals);
  EXPECT_EQ(built.uvs, substituted.uvs);
  EXPECT_EQ(built.indices, substituted.indices);
}

TEST(MeshSweep, EveryNormalRuleIsFormedFromTheSameRings) {
  const std::vector<Frame3> r = rail(6);
  const path::Polyline p = pop::profile::circle(10);
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
