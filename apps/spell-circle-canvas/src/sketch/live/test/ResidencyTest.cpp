/** @file
 * The resident set: which session comes back, and which one leaves.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/live/Residency.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

struct Square : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(120, 90);
    ctx.background({0, 0, 0, 1});
    ctx.composer.render(
        box().width(40).height(40).fill(Fill::color({0, 1, 0, 1})));
  }
};

Kind squareKind() { return kindOf<Square>(); }
const Entry kSquare{"square", "square", "Test", "", &squareKind};

/** A file for a host to watch, in a directory of its own — the host
 *  watches the headers beside a sketch as well as the sketch. */
std::filesystem::path watched(const std::string& name) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                    ("sigil_sketch_resident_" + name);
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::filesystem::path path = dir / "sketch.cpp";
  std::ofstream(path) << "// watched, never built\n";
  return path;
}

/** Opens the compiled-in square over @p path, which is what every host
 *  in this file holds: no compiler runs here. */
Residency::Open opener(const std::filesystem::path& path, int* built) {
  // opening a session can fail only on allocation
  // NOLINTNEXTLINE(bugprone-exception-escape)
  return [path, built] {
    if (built) ++*built;
    Host::Options options;
    options.sketchPath = path;
    options.assetsDir = path.parent_path();
    options.flagsFile = path.parent_path() / "no_such.rsp";
    options.compiledIn = &kSquare;
    return std::make_unique<Host>(std::move(options), fonts());
  };
}

/** Draws one frame and reports it presented, which is what fills the
 *  rolling windows a readout is taken from. */
void present(Host& host) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  host.frame(*surface->getCanvas(), 1.0 / 60.0);
  host.markPresented();
}

TEST(SketchResidency, PresentingASketchTwiceOpensItOnce) {
  // The whole point: switching away and back is not a rebuild. Setup
  // runs once per sketch rather than once per visit.
  Residency residency;
  const std::filesystem::path a = watched("a");
  const std::filesystem::path b = watched("b");
  int builtA = 0, builtB = 0;
  const Residency::Presented first =
      residency.present(a.string(), opener(a, &builtA));
  EXPECT_TRUE(first.opened);
  residency.present(b.string(), opener(b, &builtB));
  const Residency::Presented again =
      residency.present(a.string(), opener(a, &builtA));
  EXPECT_FALSE(again.opened);
  EXPECT_EQ(again.host, first.host);
  EXPECT_EQ(builtA, 1);
  EXPECT_EQ(builtB, 1);
}

TEST(SketchResidency, TheReadoutSurvivesALookAtSomethingElse) {
  // A session's rolling frame windows live in the session, so coming
  // back to one shows what it was doing rather than a ring filling from
  // zero.
  Residency residency;
  const std::filesystem::path a = watched("readout_a");
  const std::filesystem::path b = watched("readout_b");
  Host* host = residency.present(a.string(), opener(a, nullptr)).host;
  ASSERT_NE(host, nullptr);
  for (int i = 0; i < 8; ++i) present(*host);
  const double work = host->workMsAverage();
  EXPECT_GT(work, 0.0);
  residency.present(b.string(), opener(b, nullptr));
  EXPECT_EQ(
      residency.present(a.string(), opener(a, nullptr)).host->workMsAverage(),
      work);
}

TEST(SketchResidency, KeepsTheLastThreeAndDropsTheLeastRecentlyPresented) {
  Residency residency;
  EXPECT_EQ(residency.capacity(), kResidentSessions);
  std::vector<std::filesystem::path> files;
  files.reserve(4);
  for (int i = 0; i < 4; ++i)
    files.push_back(watched("evict_" + std::to_string(i)));
  for (int i = 0; i < 3; ++i)
    residency.present(files[i].string(), opener(files[i], nullptr));
  EXPECT_EQ(residency.size(), kResidentSessions);
  // Presenting the oldest again makes it the newest, so the one that
  // leaves next is the one nothing has looked at for longest — which is
  // now the second, not the first.
  residency.present(files[0].string(), opener(files[0], nullptr));
  int built = 0;
  residency.present(files[3].string(), opener(files[3], &built));
  EXPECT_EQ(built, 1);
  EXPECT_EQ(residency.size(), kResidentSessions);
  const std::vector<std::string> keys = residency.keys();
  ASSERT_EQ(keys.size(), kResidentSessions);
  EXPECT_EQ(keys[0], files[3].string());
  EXPECT_EQ(keys[1], files[0].string());
  EXPECT_EQ(keys[2], files[2].string());
}

TEST(SketchResidency, AnEvictedSketchIsOpenedAgainWhenItComesBack) {
  Residency residency;
  std::vector<std::filesystem::path> files;
  files.reserve(4);
  for (int i = 0; i < 4; ++i)
    files.push_back(watched("return_" + std::to_string(i)));
  int built = 0;
  residency.present(files[0].string(), opener(files[0], &built));
  for (int i = 1; i < 4; ++i)
    residency.present(files[i].string(), opener(files[i], nullptr));
  EXPECT_TRUE(
      residency.present(files[0].string(), opener(files[0], &built)).opened);
  EXPECT_EQ(built, 2);
}

TEST(SketchResidency, ClearingReleasesEverySession) {
  // The owner says when, because a session holding device-backed images
  // has to go while the device that made them is still up.
  Residency residency;
  const std::filesystem::path a = watched("clear");
  residency.present(a.string(), opener(a, nullptr));
  residency.clear();
  EXPECT_EQ(residency.size(), 0u);
  EXPECT_EQ(residency.presented(), nullptr);
  EXPECT_TRUE(residency.present(a.string(), opener(a, nullptr)).opened);
}

}  // namespace
