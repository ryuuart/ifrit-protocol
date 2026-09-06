/** @file
 * The words a sketch reads off its own assets directory.
 */

#include <gtest/gtest.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Drawn.h"

namespace {

namespace kit = sigil::sketch::kit;
namespace compose = sigil::compose;
using compose::Element;
using compose::Fill;
using sigil::sketch::kit::test::Drawn;
using sigil::sketch::kit::test::kTall;
using sigil::sketch::kit::test::kWide;
using sigil::sketch::kit::test::sameDrawing;
using sigil::sketch::kit::test::subject;
using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;

/** A sketch's assets directory holding one passage, so what `passage`
 *  answers can be compared to what was written. */
struct Beside {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() / "sketch_kit_passage";
  sigil::sketch::Assets store{root};
  sigil::motion::Ticker ticker;
  compose::Composer composer{ticker, fonts()};
  sigil::sketch::CanvasSpec spec;
  sigil::sketch::SketchContext ctx{composer, ticker, store,
                                   {0, 0},   &spec,  &fonts()};

  explicit Beside(std::string_view text) {
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "passages");
    std::ofstream(root / "passages" / "one.txt", std::ios::binary) << text;
  }
  ~Beside() { std::filesystem::remove_all(root); }
};

/** Narrowed for the failure message: the passage's bytes are what is
 *  being asserted, and a test framework prints them as text. */
std::string read(const std::u8string& text) {
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

TEST(SketchKitPassage, TheTextIsTheFilesWithoutItsLastNewline) {
  Beside beside("one line\nand another\n");
  EXPECT_EQ(read(kit::passage(beside.ctx, "one.txt")), "one line\nand another");
}

TEST(SketchKitPassage, AMissingPassageIsEmptyRatherThanAStandIn) {
  Beside beside("anything");
  EXPECT_TRUE(kit::passage(beside.ctx, "absent.txt").empty());
}

}  // namespace
