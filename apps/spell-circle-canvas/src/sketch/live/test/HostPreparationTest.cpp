/** @file
 * What the live host prepares before a session opens: the preparation runs
 * before the availability probe and the body, a required device's failure
 * comes before a scene3d body is made, and a refused preparation keeps the
 * session that was accepted.
 */

#include <gtest/gtest.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/frame/Frame.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "HostTestSupport.h"
#include "support/Fixtures.h"

namespace {

using namespace sigil::sketch;
using sigil::sketch::test::fonts;
using sigil::sketch::test::options;
using sigil::sketch::test::Restarted;
using sigil::sketch::test::Watched;

std::vector<std::string> preparationOrder;
bool executorReady = false;
bool preparationProbeFails = false;

struct PreparedScene2D {
  static constexpr bool needsDevice = true;
  PreparedScene2D() { preparationOrder.emplace_back("body"); }
  void setup(Scene2DContext& ctx) {
    preparationOrder.emplace_back("setup");
    ctx.canvas(120, 90);
  }
};

Kind preparedScene2DKind() {
  preparationOrder.emplace_back("kind");
  return kindOf<PreparedScene2D>();
}

bool preparedAvailable(std::string* why) {
  preparationOrder.emplace_back("probe");
  if (executorReady && !preparationProbeFails) return true;
  if (why) *why = "the executor is unavailable";
  return false;
}

TEST(SketchHost, PreparesBeforeAvailabilityAndSetupOnOpenAndRestart) {
  const Watched file("sigil_sketch_prepared");
  const Entry entry{"prepared", "prepared",           "Test",
                    "",         &preparedScene2DKind, &preparedAvailable};
  auto opts = options(file.path);
  opts.compiledIn = &entry;
  executorReady = false;
  preparationProbeFails = false;
  preparationOrder.clear();
  bool refuse = false;
  opts.prepareSession = [&](const Kind& kind) {
    EXPECT_TRUE(kind->needsDevice());
    preparationOrder.emplace_back("prepare");
    if (refuse) throw std::runtime_error("the executor preparation failed");
    executorReady = true;
  };
  Host host(std::move(opts), fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  EXPECT_TRUE(host.needsDevice());
  EXPECT_EQ(
      preparationOrder,
      (std::vector<std::string>{"kind", "prepare", "probe", "body", "setup"}));
  Session* accepted = host.session();

  preparationOrder.clear();
  refuse = true;
  EXPECT_FALSE(host.restartSession());
  EXPECT_EQ(preparationOrder, (std::vector<std::string>{"prepare"}));
  EXPECT_EQ(host.session(), accepted);
  EXPECT_NE(host.errorLog().find("preparation failed"), std::string::npos);

  preparationOrder.clear();
  refuse = false;
  preparationProbeFails = true;
  EXPECT_FALSE(host.restartSession());
  EXPECT_EQ(preparationOrder, (std::vector<std::string>{"prepare", "probe"}));
  EXPECT_EQ(host.session(), accepted);

  preparationOrder.clear();
  preparationProbeFails = false;
  EXPECT_TRUE(host.restartSession());
  EXPECT_EQ(preparationOrder,
            (std::vector<std::string>{"prepare", "probe", "body", "setup"}));
}

struct PreparedScene3D {
  static inline int bodies = 0;
  static inline int setups = 0;
  PreparedScene3D() { ++bodies; }
  void setup(Scene3DContext&) { ++setups; }
  sigil::world::Frame describe() { return {}; }
};

Kind preparedScene3DKind() { return kindOf<PreparedScene3D>(); }

TEST(SketchHost, ARequiredDeviceFailurePrecedesScene3DBodyConstruction) {
  const Watched file("sigil_sketch_required_device");
  const Entry entry{"prepared_set", "prepared_set", "Test", "",
                    &preparedScene3DKind};
  auto opts = options(file.path);
  opts.compiledIn = &entry;
  PreparedScene3D::bodies = 0;
  PreparedScene3D::setups = 0;
  opts.prepareSession = [](const Kind& kind) {
    EXPECT_TRUE(kind->needsDevice());
    throw std::runtime_error("the required device is unavailable");
  };
  Host host(std::move(opts), fonts());
  EXPECT_FALSE(host.live());
  EXPECT_EQ(PreparedScene3D::bodies, 0);
  EXPECT_EQ(PreparedScene3D::setups, 0);
  EXPECT_NE(host.errorLog().find("required device"), std::string::npos);
}

TEST(SketchHost, AScene3DWithNoDeviceToDrawOnIsUnavailableBeforeItsBodyIsMade) {
  // A host that brought no device up opens no scene3d sketch: the session is
  // refused with the one message every door a scene3d sketch is refused at
  // speaks, and the body is never made.
  const Watched file("sigil_sketch_set_without_device");
  const Entry entry{"prepared_set", "prepared_set", "Test", "",
                    &preparedScene3DKind};
  useRuntime({});
  std::string why;
  ASSERT_FALSE(runtimeAvailable(&why));
  EXPECT_FALSE(entry.available());
  auto opts = options(file.path);
  opts.compiledIn = &entry;
  PreparedScene3D::bodies = 0;
  PreparedScene3D::setups = 0;
  Host host(std::move(opts), fonts());
  EXPECT_FALSE(host.live());
  EXPECT_EQ(PreparedScene3D::bodies, 0);
  EXPECT_EQ(PreparedScene3D::setups, 0);
  EXPECT_NE(host.errorLog().find(why), std::string::npos) << host.errorLog();
}

TEST(SketchHost, ARejectedScene3DReplacementKeepsTheCanvasAndCaptureBackend) {
  const Watched file("sigil_sketch_candidate_device");
  auto python = file.path;
  python.replace_extension(".py");
  std::ofstream(python) << "source handled by importer\n";
  auto opts = options(python);
  opts.compiledIn = nullptr;
  static bool requestSet = false;
  requestSet = false;
  opts.pythonLoader = [](const std::filesystem::path&) {
    return requestSet ? kindOf<PreparedScene3D>() : kindOf<Restarted>();
  };
  std::vector<bool> requirements;
  opts.prepareSession = [&](const Kind& kind) {
    requirements.push_back(kind->needsDevice());
    if (kind->needsDevice())
      throw std::runtime_error("the candidate device is unavailable");
  };
  Restarted::failSetup = false;
  PreparedScene3D::bodies = 0;
  PreparedScene3D::setups = 0;
  Host host(std::move(opts), fonts());
  host.poll();
  ASSERT_TRUE(host.live()) << host.errorLog();
  EXPECT_FALSE(host.needsDevice());
  Session* accepted = host.session();
  int captures = 0;
  host.setCaptureBackend({[&](const SkImageInfo& info) {
    ++captures;
    return SkSurfaces::Raster(info);
  }});

  requestSet = true;
  std::filesystem::last_write_time(
      python,
      std::filesystem::last_write_time(python) + std::chrono::seconds(1));
  host.poll();
  EXPECT_EQ(requirements, (std::vector<bool>{false, true}));
  EXPECT_EQ(host.session(), accepted);
  EXPECT_EQ(host.kind(), "scene2d");
  EXPECT_FALSE(host.needsDevice());
  EXPECT_EQ(PreparedScene3D::bodies, 0);
  EXPECT_EQ(PreparedScene3D::setups, 0);
  EXPECT_FALSE(host.still().drawsNothing());
  EXPECT_EQ(captures, 1);
  EXPECT_TRUE(host.restartSession());
  EXPECT_EQ(requirements, (std::vector<bool>{false, true, false}));
  EXPECT_FALSE(host.still().drawsNothing());
  EXPECT_EQ(captures, 2);
}

TEST(SketchHost, UnknownPreparationFailuresPreserveTheAcceptedSession) {
  const Watched file("sigil_sketch_unknown_preparation");
  auto opts = options(file.path);
  bool refuse = false;
  opts.prepareSession = [&](const Kind&) {
    if (refuse) throw 7;
  };
  Host host(std::move(opts), fonts());
  ASSERT_TRUE(host.live());
  Session* accepted = host.session();
  refuse = true;
  EXPECT_FALSE(host.restartSession());
  EXPECT_EQ(host.session(), accepted);
  EXPECT_NE(host.errorLog().find("unknown exception"), std::string::npos);
}

}  // namespace
