/** @file
 * A picture beside a sketch the live host compiled and loaded while it
 * runs: the guest is an image of its own, whose `media::Image` is not the
 * host's type identity, and its `hub().load<media::Image>` still reaches
 * the decoder the host registered — by the meaning's name.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilio/source/Sink.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/image/Encode.h>
#include <sigilsketch/live/Host.h>

#include <cstddef>
#include <filesystem>
#include <vector>

#include "../BuildCache.h"
#include "Fixture.h"
#include "ScratchDir.h"
#include "support/Fixtures.h"
#include "support/StateRoot.h"

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
  ASSERT_TRUE(
      sigil::io::writeBytes(sketches.path / "image_asking_guest" / "mark.png",
                            png.data(), png.size()));

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

TEST(SketchGuestPicture, ACachedBuildWithResourceWarningsDoesNotCompileAgain) {
  const sketch::test::StateRoot state("sigil_sketch_guest_cache_warning");
  sigil::test::ScratchDir sketches("sigil_sketch_guest_missing_picture");
  sketches.write("image_asking_guest/image_asking_guest.cpp",
                 "// adopted from the guest image\n");
  sketches.write("image_asking_guest/warn-on-missing-picture",
                 "report warnings\n");
  sketches.write("flags.rsp", "");

  sketch::Host::Options options;
  options.sketchPath =
      sketches.path / "image_asking_guest" / "image_asking_guest.cpp";
  options.assetsDirectory = sketches.path / "assets";
  options.flagsFile = sketches.path / "flags.rsp";
  options.compiler = sketch::test::guestCompiler(sketches.path / "compiler.sh",
                                                 SIGIL_SKETCH_IMAGE_GUEST);
  options.clock = sigil::motion::ClockPolicy::Advance;
  ASSERT_NE(options.hostStamp, std::filesystem::file_time_type{});
  {
    sketch::Host host(options, sketch::test::fonts());
    ASSERT_TRUE(sketch::test::buildOnce(host));
    ASSERT_TRUE(host.live()) << host.errorLog();
    ASSERT_NE(host.errorLog().find("mark.png"), std::string::npos);
  }
  std::vector<std::filesystem::path> artifacts;
  for (const auto& file :
       std::filesystem::directory_iterator(sketch::buildCacheDirectory()))
    if (file.path().extension() == ".bin") artifacts.push_back(file.path());
  ASSERT_EQ(artifacts.size(), 1u);

  sketch::Host cached(options, sketch::test::fonts());
  ASSERT_TRUE(sketch::test::buildOnce(cached));
  ASSERT_TRUE(cached.live()) << cached.errorLog();
  EXPECT_EQ(cached.generation(), 1);
  EXPECT_NE(cached.status().find("cached build"), std::string::npos);
  EXPECT_NE(cached.errorLog().find("mark.png"), std::string::npos);
  EXPECT_TRUE(std::filesystem::exists(artifacts.front()));
  auto* accepted = cached.session();
  cached.poll();
  EXPECT_FALSE(cached.compiling());
  EXPECT_EQ(cached.generation(), 1);
  EXPECT_EQ(cached.session(), accepted);
}

}  // namespace

#endif
