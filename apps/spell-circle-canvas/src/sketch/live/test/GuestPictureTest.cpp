/** @file
 * A picture beside a sketch the live host compiled and loaded while it
 * runs: the guest is an image of its own, whose `media::Image` is not the
 * host's type identity, and its `hub().load<media::Image>` still reaches
 * the decoder the host registered — by the meaning's name.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilio/source/Sink.h>
#include <sigilmedia/image/Encode.h>
#include <sigilsketch/live/Host.h>

#include <cstddef>
#include <filesystem>
#include <vector>

#include "Fixture.h"
#include "ScratchDir.h"
#include "support/Fixtures.h"

#ifdef SIGIL_SKETCH_IMAGE_GUEST

namespace {

namespace sketch = sigil::sketch;

TEST(SketchGuestPicture, AnAdoptedBuildLoadsAPictureThroughTheHostsDecoder) {
  sigil::test::ScratchDir sketches("sigil_sketch_guest_picture");
  sketches.write("image_asking_guest/image_asking_guest.cpp",
                 "// adopted from the guest image, never compiled\n");
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(12, 7));
  bitmap.eraseColor(SK_ColorGREEN);
  const std::vector<std::byte> png =
      sigil::media::encode(bitmap.pixmap(), sigil::media::Format::Png);
  ASSERT_FALSE(png.empty());
  ASSERT_TRUE(sigil::io::writeBytes(
      sketches.path / "image_asking_guest" / "mark.png", png.data(),
      png.size()));

  sketch::Host::Options options;
  options.sketchPath =
      sketches.path / "image_asking_guest" / "image_asking_guest.cpp";
  options.assetsDirectory = std::filesystem::temp_directory_path();
  options.sketchesDirectory = sketches.path;
  options.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
  options.compiler = sketch::test::guestCompiler(sketches.path / "compiler.sh",
                                                SIGIL_SKETCH_IMAGE_GUEST);
  options.compiledIn = nullptr;
  options.clock = sigil::motion::ClockPolicy::Advance;
  sketch::Host host(std::move(options), sketch::test::fonts());

  ASSERT_TRUE(sketch::test::buildOnce(host)) << "the build never finished";
  ASSERT_TRUE(host.live()) << host.errorLog();
  // The guest's canvas is the picture's size only when its load answered.
  EXPECT_EQ(host.canvasSize(), SkSize::Make(12, 7));
}

}  // namespace

#endif
