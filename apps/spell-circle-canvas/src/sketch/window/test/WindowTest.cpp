/** @file
 * A window opened, handed a host, pumped, photographed and closed, with
 * no application around it: headless anywhere, and on screen where a
 * window server and a graphics device are there to put it.
 */

#include <gtest/gtest.h>
#include <sigilcompose/core/Factories.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/window/Window.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

#include "Fonts.h"
#include "ScratchDir.h"

namespace {

using namespace sigil::sketch;
using namespace std::chrono_literals;

/** One green square on black, set up once per session. */
struct Square {
  static inline int setups = 0;

  void setup(Scene2DContext& ctx) {
    ++setups;
    ctx.canvas(120, 90);
    ctx.background({0, 0, 0, 1});
    ctx.composer.render(sigil::compose::box().width(40).height(40).fill(
        sigil::compose::Fill::color({0, 1, 0, 1})));
  }
};

/** A host over the square, opened from the kind itself: no file. */
std::unique_ptr<Host> squareHost() {
  Host::Options options;
  options.kind = kindOf<Square>();
  options.assetsDirectory = std::filesystem::temp_directory_path();
  return std::make_unique<Host>(std::move(options), sigil::test::fonts());
}

TEST(SketchWindow, AHostOpenedOnAKindIsLiveWithNoFileBehindIt) {
  const int before = Square::setups;
  const std::unique_ptr<Host> host = squareHost();
  EXPECT_TRUE(host->live()) << host->errorLog();
  EXPECT_TRUE(host->sketchPath().empty());
  EXPECT_EQ(Square::setups, before + 1);
  host->poll();
  EXPECT_TRUE(host->restartSession());
  EXPECT_EQ(Square::setups, before + 2);
}

TEST(SketchWindow, AHeadlessWindowStepsItsHostAndWritesAStill) {
  WindowOptions options;
  options.headless = true;
  std::string why;
  std::unique_ptr<Window> window = Window::open(options, &why);
  ASSERT_TRUE(window) << why;
  EXPECT_FALSE(window->still("unwritten.png", 1.0f)) << "no host is shown";
  window->show(squareHost());
  ASSERT_NE(window->host(), nullptr);
  for (int frame = 0; frame < 3; ++frame) EXPECT_TRUE(window->pump(0ms));
  EXPECT_TRUE(window->host()->live());

  const sigil::test::ScratchDir scratch("sigil_sketch_window_headless");
  const std::filesystem::path still = scratch.path / "square.png";
  ASSERT_TRUE(window->still(still, 1.0f));
  EXPECT_GT(std::filesystem::file_size(still), 0u);

  window->close();
  EXPECT_FALSE(window->isOpen());
  EXPECT_FALSE(window->pump(0ms));
  EXPECT_EQ(window->host(), nullptr);
}

TEST(SketchWindow, AWindowOnScreenIsOpenedOnlyOnTheMainThread) {
  std::unique_ptr<Window> window;
  std::string why;
  std::thread elsewhere([&] { window = Window::open(WindowOptions{}, &why); });
  elsewhere.join();
  EXPECT_FALSE(window);
  EXPECT_NE(why.find("main thread"), std::string::npos) << why;
}

TEST(SketchWindowOnScreen, OpensShowsDrawsAndClosesWithoutAnApplication) {
  WindowOptions options;
  options.width = 240;
  options.height = 180;
  options.title = "sketch_test";
  std::string why;
  std::unique_ptr<Window> window = Window::open(options, &why);
  if (!window) GTEST_SKIP() << "no window could open here: " << why;
  window->show(squareHost());
  const auto deadline = std::chrono::steady_clock::now() + 10s;
  while (window->host()->presentedFrames() < 3 &&
         std::chrono::steady_clock::now() < deadline)
    ASSERT_TRUE(window->pump(16ms));
  if (window->host()->presentedFrames() == 0)
    GTEST_SKIP() << "the window drew no frame here: no graphics device";
  const sigil::test::ScratchDir scratch("sigil_sketch_window_on_screen");
  const std::filesystem::path still = scratch.path / "square.png";
  ASSERT_TRUE(window->still(still, 2.0f));
  EXPECT_GT(std::filesystem::file_size(still), 0u);
  window->close();
  EXPECT_FALSE(window->pump(0ms));
}

}  // namespace
