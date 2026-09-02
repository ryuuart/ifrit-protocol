/** @file
 * The resident set: which session comes back, and which one leaves.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/live/Residency.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Fixture.h"
#include "Support.h"

namespace {

using namespace sigil::sketch;
using sigil::sketch::test::fonts;
using sigil::sketch::test::kSquare;
using sigil::sketch::test::Watched;

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

/** @p count files, each in a directory of its own, for a residency to
 *  hold at once. */
std::vector<Watched> watchedFiles(const std::string& label, int count) {
  std::vector<Watched> files;
  files.reserve((size_t)count);
  for (int i = 0; i < count; ++i)
    files.emplace_back(label + "_" + std::to_string(i));
  return files;
}

TEST(SketchResidency, PresentingASketchTwiceOpensItOnce) {
  // The whole point: switching away and back is not a rebuild. Setup
  // runs once per sketch rather than once per visit.
  Residency residency;
  const std::vector<Watched> files =
      watchedFiles("sigil_sketch_resident_twice", 2);
  const std::string a = files[0].path.string();
  const std::string b = files[1].path.string();
  int builtA = 0, builtB = 0;
  const Residency::Presented first =
      residency.present(a, opener(files[0].path, &builtA));
  EXPECT_TRUE(first.opened);
  residency.present(b, opener(files[1].path, &builtB));
  const Residency::Presented again =
      residency.present(a, opener(files[0].path, &builtA));
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
  const std::vector<Watched> files =
      watchedFiles("sigil_sketch_resident_readout", 2);
  const std::string a = files[0].path.string();
  Host* host = residency.present(a, opener(files[0].path, nullptr)).host;
  ASSERT_NE(host, nullptr);
  for (int i = 0; i < 8; ++i) present(*host);
  const double work = host->workMsAverage();
  EXPECT_GT(work, 0.0);
  residency.present(files[1].path.string(), opener(files[1].path, nullptr));
  EXPECT_EQ(residency.present(a, opener(files[0].path, nullptr))
                .host->workMsAverage(),
            work);
}

TEST(SketchResidency, KeepsTheLastThreeAndDropsTheLeastRecentlyPresented) {
  Residency residency;
  EXPECT_EQ(residency.capacity(), kResidentSessions);
  const std::vector<Watched> files =
      watchedFiles("sigil_sketch_resident_evict", 4);
  for (int i = 0; i < 3; ++i)
    residency.present(files[i].path.string(), opener(files[i].path, nullptr));
  EXPECT_EQ(residency.size(), kResidentSessions);
  // Presenting the oldest again makes it the newest, so the one that
  // leaves next is the one nothing has looked at for longest — which is
  // now the second, not the first.
  residency.present(files[0].path.string(), opener(files[0].path, nullptr));
  int built = 0;
  residency.present(files[3].path.string(), opener(files[3].path, &built));
  EXPECT_EQ(built, 1);
  EXPECT_EQ(residency.size(), kResidentSessions);
  const std::vector<std::string> keys = residency.keys();
  ASSERT_EQ(keys.size(), kResidentSessions);
  EXPECT_EQ(keys[0], files[3].path.string());
  EXPECT_EQ(keys[1], files[0].path.string());
  EXPECT_EQ(keys[2], files[2].path.string());
}

TEST(SketchResidency, AnEvictedSketchIsOpenedAgainWhenItComesBack) {
  Residency residency;
  const std::vector<Watched> files =
      watchedFiles("sigil_sketch_resident_return", 4);
  int built = 0;
  residency.present(files[0].path.string(), opener(files[0].path, &built));
  for (int i = 1; i < 4; ++i)
    residency.present(files[i].path.string(), opener(files[i].path, nullptr));
  EXPECT_TRUE(
      residency.present(files[0].path.string(), opener(files[0].path, &built))
          .opened);
  EXPECT_EQ(built, 2);
}

TEST(SketchResidency, ClearingReleasesEverySession) {
  // The owner says when, because a session holding device-backed images
  // has to go while the device that made them is still up.
  Residency residency;
  const Watched file("sigil_sketch_resident_clear");
  const std::string a = file.path.string();
  residency.present(a, opener(file.path, nullptr));
  residency.clear();
  EXPECT_EQ(residency.size(), 0u);
  EXPECT_EQ(residency.presented(), nullptr);
  EXPECT_TRUE(residency.present(a, opener(file.path, nullptr)).opened);
}

}  // namespace
