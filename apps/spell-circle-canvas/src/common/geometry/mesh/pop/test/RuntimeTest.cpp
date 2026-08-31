/** @file
 * The cook's runtime seam: the built-in value is one value however it is
 * reached, a substituted executor receives the cook instead, and an
 * operator a runtime does not support stops the cook with a message
 * naming both.
 */

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "sigilgeometry/mesh/pop/Pop.h"

using namespace sigil::geometry::mesh;

namespace {

std::vector<glm::vec3> ring() {
  return {{-100, 0, 0}, {0, 60, 40}, {100, 0, 0}, {0, -60, -40}};
}

/** An executor that answers a cloud of its own rather than cooking, and
 *  records that it was asked. Two of them compare by the label they
 *  carry, which is what makes a Runtime holding one a comparable
 *  value. */
struct Recorder : pop::Executor {
  explicit Recorder(std::string called, bool takesEverything = true)
      : label(std::move(called)), everything(takesEverything) {}

  std::string label;
  bool everything;
  mutable int cooks = 0;

  bool operator==(const Recorder& o) const {
    return label == o.label && everything == o.everything;
  }

  std::string name() const override { return label; }
  // The narrow executor runs generators and nothing else, which is the
  // shape of a backend that can seed lanes but not filter them.
  bool supports(const pop::Op& op) const override {
    return everything || std::holds_alternative<pop::SplineScatter>(op);
  }
  Cloud cook(const pop::Chain&) const override {
    ++cooks;
    Cloud out;
    out.positions.assign(3, glm::vec3{1, 2, 3});
    return out;
  }
};

}  // namespace

TEST(PopRuntime, BuiltInIsOneValue) {
  EXPECT_TRUE((bool)pop::Runtime::cpu());
  EXPECT_EQ(pop::Runtime::cpu(), pop::Runtime::cpu());
}

TEST(PopRuntime, ComparesByModelValue) {
  const pop::Runtime a{Recorder{"a"}};
  const pop::Runtime b{Recorder{"a"}};
  const pop::Runtime c{Recorder{"c"}};
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, pop::Runtime::cpu());
  EXPECT_NE(pop::Runtime(), a);
}

// The runtime is the whole of the switch: the same chain, the same
// sinks, a different executor.
TEST(PopRuntime, CookRoutesToTheRuntimeItIsGiven) {
  const pop::Chain chain = pop::on(ring()).count(64).jitter(3);
  const pop::Runtime runtime{Recorder{"counting"}};

  const Cloud cloud = pop::cook(chain, runtime);
  EXPECT_EQ(cloud.size(), 3u);
  EXPECT_EQ(cloud.positions[0], (glm::vec3{1, 2, 3}));

  const auto* recorder = static_cast<const Recorder*>(runtime.get());
  ASSERT_NE(recorder, nullptr);
  EXPECT_EQ(recorder->cooks, 1);

  // The mesh-forming sinks stand on the same cook, so they route too.
  (void)pop::cookMesh(chain, quad(2, 2), runtime);
  EXPECT_EQ(recorder->cooks, 2);
}

TEST(PopRuntime, TheDefaultRuntimeIsTheBuiltInOne) {
  const pop::Chain chain = pop::on(ring()).count(128).jitter(5);
  const Cloud implicit = pop::cook(chain);
  const Cloud explicitly = pop::cook(chain, pop::Runtime::cpu());
  ASSERT_EQ(implicit.size(), explicitly.size());
  EXPECT_EQ(implicit.positions, explicitly.positions);
}

TEST(PopRuntime, AnUnsupportedOperatorStopsTheCookByName) {
  const pop::Chain chain = pop::on(ring()).count(32).jitter(4);
  const pop::Runtime narrow{Recorder{"narrow", false}};

  try {
    (void)pop::cook(chain, narrow);
    FAIL() << "a chain the runtime cannot run must not cook";
  } catch (const std::runtime_error& e) {
    const std::string message = e.what();
    EXPECT_NE(message.find("narrow"), std::string::npos) << message;
    EXPECT_NE(message.find("Jitter"), std::string::npos) << message;
  }

  // Nothing reached the executor: the check runs before the dispatch.
  EXPECT_EQ(static_cast<const Recorder*>(narrow.get())->cooks, 0);

  // A chain of only what it declares cooks without complaint.
  EXPECT_NO_THROW((void)pop::cook(pop::on(ring()).count(32), narrow));
}

TEST(PopRuntime, EveryOperatorNamesItself) {
  EXPECT_EQ(pop::opName(pop::Op{pop::SplineScatter{}}), "SplineScatter");
  EXPECT_EQ(pop::opName(pop::Op{pop::Jitter{}}), "Jitter");
  EXPECT_EQ(pop::opName(pop::Op{pop::Deform{}}), "Deform");
  EXPECT_EQ(pop::opName(pop::Op{pop::PointSet{}}), "PointSet");
}
