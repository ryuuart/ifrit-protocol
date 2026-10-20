/** @file
 * The live host's session: a compiled-in sketch opened without building it,
 * restarted, kept through a failed setup, stopped by a failed frame, fed
 * the pointer and keys before a step, and drawn.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/live/Host.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

#include "HostTestSupport.h"
#include "support/Fixtures.h"

namespace {

using namespace sigil::sketch;
using sigil::sketch::test::fonts;
using sigil::sketch::test::kRestarted;
using sigil::sketch::test::options;
using sigil::sketch::test::Restarted;
using sigil::sketch::test::Watched;

TEST(SketchHost, OpensACompiledInSketchWithoutBuildingIt) {
  const Watched file("sigil_sketch_host_unedited");
  Host host(options(file.path), fonts());
  EXPECT_TRUE(host.live());
  EXPECT_FALSE(host.compiling());
  EXPECT_EQ(host.canvasSize(), SkSize::Make(120, 90));
  // Polling an unedited file must not kick a build: the sketch is
  // already here, and rebuilding it would only make selecting one slow.
  host.poll();
  EXPECT_FALSE(host.compiling());
  EXPECT_TRUE(host.errorLog().empty());
}

TEST(SketchHost, RestartsTheRuntimeSessionWithoutBuildingAgain) {
  const Watched file("sigil_sketch_host_restart");
  Host::Options opts = options(file.path);
  opts.compiledIn = &kRestarted;
  Restarted::setups = 0;
  Host host(std::move(opts), fonts());
  ASSERT_TRUE(host.live());
  EXPECT_EQ(Restarted::setups, 1);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  host.frame(*surface->getCanvas(), 1.0 / 60.0);
  EXPECT_GT(host.frameTimes().work().mean(), sigil::motion::Duration::zero());

  EXPECT_TRUE(host.restartSession());
  EXPECT_EQ(Restarted::setups, 2);
  EXPECT_TRUE(host.frameTimes().work().empty());
  EXPECT_FALSE(host.compiling());
}

TEST(SketchHost, FailedSetupPreservesTheRunningSession) {
  const Watched file("sigil_sketch_host_failed_setup");
  Host::Options opts = options(file.path);
  opts.compiledIn = &kRestarted;
  Restarted::failSetup = false;
  Host host(std::move(opts), fonts());
  Session* previous = host.session();
  ASSERT_NE(previous, nullptr);

  Restarted::failSetup = true;
  EXPECT_FALSE(host.restartSession());
  Restarted::failSetup = false;
  EXPECT_EQ(host.session(), previous);
  EXPECT_TRUE(host.live());
  EXPECT_NE(host.errorLog().find("setup rejected"), std::string::npos);
  EXPECT_NE(host.status().find("setup failed"), std::string::npos)
      << host.status();
  EXPECT_TRUE(host.restartSession());
  EXPECT_TRUE(host.errorLog().empty());
  EXPECT_EQ(host.status(), "live · compiled in");
}

struct FailingFrame {
  static inline int updates = 0;
  void setup(Scene2DContext& ctx) { ctx.canvas(120, 90); }
  void update() {
    ++updates;
    throw std::runtime_error("the frame callback failed");
  }
};
Kind failingFrameKind() { return kindOf<FailingFrame>(); }
const Entry kFailingFrame{"failed_frame", "failed_frame", "Test", "",
                          &failingFrameKind};

TEST(SketchHost, FailedFramesStopCallbacksAndRefuseCaptureUntilReload) {
  const Watched file("sigil_sketch_host_failed_frame");
  Host::Options opts = options(file.path);
  opts.compiledIn = &kFailingFrame;
  FailingFrame::updates = 0;
  Host host(std::move(opts), fonts());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  EXPECT_FALSE(host.frame(*surface->getCanvas(), 1.0 / 60.0));
  EXPECT_NE(host.errorLog().find("frame callback failed"), std::string::npos);
  EXPECT_FALSE(host.frame(*surface->getCanvas(), 1.0 / 60.0));
  EXPECT_EQ(FailingFrame::updates, 1);
  const auto capture = file.dir.path / "failed.png";
  EXPECT_FALSE(host.capture(capture));
  EXPECT_FALSE(std::filesystem::exists(capture));
  EXPECT_NE(host.status().find("failed"), std::string::npos) << host.status();
  EXPECT_TRUE(host.restartSession());
  EXPECT_TRUE(host.errorLog().empty());
  // The status describes the session now running, not the one that failed.
  EXPECT_EQ(host.state(), Host::State::Live);
  EXPECT_EQ(host.status().find("failed"), std::string::npos) << host.status();
  EXPECT_EQ(host.status(), "live · compiled in");
}

/** A body with no paint program, reading the pointer and the keyboard
 *  off its context in update. */
struct ReadsInput {
  static inline sigil::compose::PaintContext::Pointer pointer;
  static inline std::string key;
  static inline bool keyDown = false;
  void setup(Scene2DContext& ctx) { ctx.canvas(120, 90); }
  void update(double, Scene2DContext& ctx) {
    pointer = ctx.pointer();
    key = ctx.keys().key;
    keyDown = ctx.keys().pressed;
  }
};
Kind readsInputKind() { return kindOf<ReadsInput>(); }
const Entry kReadsInput{"reads_input", "reads_input", "Test", "",
                        &readsInputKind};

TEST(SketchHost, UpdateReadsThePointerAndKeysFedBeforeTheStep) {
  const Watched file("sigil_sketch_host_reads_input");
  Host::Options opts = options(file.path);
  opts.compiledIn = &kReadsInput;
  ReadsInput::pointer = {};
  ReadsInput::key.clear();
  ReadsInput::keyDown = false;
  Host host(std::move(opts), fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  ASSERT_TRUE(host.frame(*surface->getCanvas(), 1.0 / 60.0));
  EXPECT_FALSE(ReadsInput::pointer.pressed);
  EXPECT_EQ(ReadsInput::pointer.at, glm::vec2(0, 0));

  host.session()->pointer(30, 40, true);
  host.session()->key("ArrowLeft", 37, true);
  ASSERT_TRUE(host.frame(*surface->getCanvas(), 1.0 / 60.0));
  EXPECT_TRUE(ReadsInput::pointer.pressed);
  EXPECT_EQ(ReadsInput::pointer.at, glm::vec2(30, 40));
  EXPECT_EQ(ReadsInput::key, "ArrowLeft");
  EXPECT_TRUE(ReadsInput::keyDown);

  host.session()->pointer(50, 10, false);
  host.session()->key("ArrowLeft", 37, false);
  ASSERT_TRUE(host.frame(*surface->getCanvas(), 1.0 / 60.0));
  EXPECT_FALSE(ReadsInput::pointer.pressed);
  EXPECT_EQ(ReadsInput::pointer.at, glm::vec2(50, 10));
  EXPECT_FALSE(ReadsInput::keyDown);
}

TEST(SketchHost, DrawsTheSketchItOpened) {
  const Watched file("sigil_sketch_host_draws");
  Host host(options(file.path), fonts());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  surface->getCanvas()->clear(SK_ColorBLACK);
  EXPECT_TRUE(host.frame(*surface->getCanvas(), 1.0 / 60.0));
  SkBitmap bitmap;
  bitmap.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
  EXPECT_EQ(bitmap.getColor(5, 5), SK_ColorGREEN);
}

TEST(SketchHost, ReportsWaitingWhenNothingHasLoaded) {
  Host::Options opts;
  opts.sketchPath = std::filesystem::temp_directory_path() / "absent.cpp";
  opts.assetsDirectory = std::filesystem::temp_directory_path();
  opts.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
  Host host(std::move(opts), fonts());
  EXPECT_FALSE(host.live());
  EXPECT_EQ(host.state(), Host::State::Waiting);
  // A source file that is not there cannot be built, and the host must
  // say so by standing still rather than by spawning a compiler.
  host.poll();
  EXPECT_FALSE(host.compiling());
}

}  // namespace
