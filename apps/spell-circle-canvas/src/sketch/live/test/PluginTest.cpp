/** @file
 * External native artifacts load without a compiler and failed replacement
 * leaves the accepted session and its mapped code usable.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilio/source/Sink.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/image/Encode.h>
#include <sigilsketch/live/Host.h>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "../BuildCache.h"
#include "ScratchDir.h"
#include "support/Fixtures.h"

#ifdef SIGIL_SKETCH_IMAGE_GUEST
namespace {

namespace sketch = sigil::sketch;

void stamp(const std::filesystem::path& path,
           std::string_view identity = sketch::hostBuildIdentity()) {
  std::ifstream input(path, std::ios::binary);
  const std::string bytes(std::istreambuf_iterator<char>(input), {});
  constexpr std::string_view marker = "SIGIL_SKETCH_METADATA_BEGIN\n";
  const auto begin = bytes.find(marker);
  ASSERT_NE(begin, std::string::npos);
  const auto end = bytes.find('\0', begin + marker.size());
  ASSERT_NE(end, std::string::npos);
  std::string payload =
      bytes.substr(begin + marker.size(), end - begin - marker.size());
  payload.replace(0, payload.find('\n'), identity);
  std::ofstream(path.string() + ".sigil-build")
      << "sigil-sketch-plugin-2\n"
      << sketch::buildDigest(bytes) << '\n'
      << payload << '\n';
}

void publish(const std::filesystem::path& guest,
             const std::filesystem::path& path) {
  std::filesystem::copy_file(guest, path,
                             std::filesystem::copy_options::overwrite_existing);
  stamp(path);
}

void picture(const std::filesystem::path& path, int width) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, 7));
  bitmap.eraseColor(SK_ColorGREEN);
  const std::vector<std::byte> png =
      sigil::media::encode(bitmap.pixmap(), sigil::media::Format::Png);
  ASSERT_TRUE(sigil::io::writeBytes(path, png.data(), png.size()));
}

sketch::Host::Options options(const std::filesystem::path& plugin) {
  sketch::Host::Options result;
  result.pluginPath = plugin;
  result.compiler = "/no/compiler/is/needed";
  result.flagsFile = "/no/flags/are/needed";
  result.clock = sigil::motion::ClockPolicy::Advance;
  // Disagreeing bytes are reported on the second poll that sees them.
  result.pluginPublicationGrace = std::chrono::milliseconds(0);
  return result;
}

TEST(SketchPlugin, LoadsWithoutACompilerAndMountsLocalAssets) {
  sigil::test::ScratchDir files("sigil_plugin_assets");
  const auto path = files.path / "scene.dylib";
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  picture(files.path / "mark.png", 12);
  sketch::Host host(options(path), sketch::test::fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  EXPECT_FALSE(host.compiling());
  EXPECT_EQ(host.canvasSize(), SkSize::Make(12, 7));
  host.poll();
  EXPECT_TRUE(host.errorLog().empty());
}

TEST(SketchPlugin, ASameStemDirectoryKeepsResourcesBesideTheModule) {
  sigil::test::ScratchDir files("sigil_plugin_resources");
  const auto path = files.path / "scene" / "scene.dylib";
  std::filesystem::create_directories(path.parent_path());
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  picture(path.parent_path() / "assets" / "mark.png", 20);
  sketch::Host host(options(path), sketch::test::fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  EXPECT_EQ(host.canvasSize(), SkSize::Make(20, 7));
}

TEST(SketchPlugin, BadBuildAndChangedBytesPreserveTheAcceptedSession) {
  sigil::test::ScratchDir files("sigil_plugin_replacement");
  const auto path = files.path / "scene.dylib";
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  picture(files.path / "mark.png", 12);
  sketch::Host host(options(path), sketch::test::fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  auto* accepted = host.session();
  stamp(path, "a different framework build");
  host.poll();
  EXPECT_EQ(host.session(), accepted);
  EXPECT_NE(host.errorLog().find("manifest mismatch"), std::string::npos);
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  std::ofstream(path, std::ios::binary | std::ios::app)
      << "changed after publication";
  host.poll();
  EXPECT_EQ(host.errorLog().find("does not match"), std::string::npos);
  host.poll();
  EXPECT_EQ(host.session(), accepted);
  EXPECT_NE(host.errorLog().find("does not match"), std::string::npos);
  EXPECT_TRUE(host.restartSession());
  EXPECT_EQ(host.canvasSize(), SkSize::Make(12, 7));
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  host.poll();
  EXPECT_TRUE(host.errorLog().empty()) << host.errorLog();
  EXPECT_EQ(host.canvasSize(), SkSize::Make(12, 7));
}

TEST(SketchPlugin,
     PreparationRunsBeforeEachOpenAndAFailedReplacementKeepsTheSession) {
  sigil::test::ScratchDir files("sigil_plugin_preparation");
  const auto path = files.path / "scene.dylib";
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  auto preparedOptions = options(path);
  int preparations = 0;
  bool refuse = false;
  preparedOptions.prepareSession = [&](const sketch::Kind&) {
    ++preparations;
    if (refuse) throw std::runtime_error("the executor could not be prepared");
    if (preparations == 1) picture(files.path / "mark.png", 20);
  };
  sketch::Host host(std::move(preparedOptions), sketch::test::fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  EXPECT_EQ(preparations, 1);
  EXPECT_EQ(host.canvasSize(), SkSize::Make(20, 7));
  auto* accepted = host.session();

  refuse = true;
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  host.poll();
  EXPECT_EQ(preparations, 2);
  EXPECT_EQ(host.session(), accepted);
  EXPECT_TRUE(host.live());
  EXPECT_NE(host.errorLog().find("could not be prepared"), std::string::npos);

  refuse = false;
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  host.poll();
  EXPECT_EQ(preparations, 3);
  EXPECT_TRUE(host.live());
  EXPECT_TRUE(host.errorLog().empty()) << host.errorLog();
  EXPECT_TRUE(host.restartSession());
  EXPECT_EQ(preparations, 4);
}

TEST(SketchPlugin, ThrowingFactoriesAndMetadataKeepMappedExceptionCode) {
  sigil::test::ScratchDir files("sigil_plugin_exceptions");
  const auto path = files.path / "scene.dylib";
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  sketch::Host host(options(path), sketch::test::fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  for (const char* guest :
       {SIGIL_SKETCH_FACTORY_GUEST, SIGIL_SKETCH_METADATA_GUEST}) {
    auto* accepted = host.session();
    publish(guest, path);
    EXPECT_NO_THROW(host.poll());
    EXPECT_EQ(host.session(), accepted);
    EXPECT_NE(host.errorLog().find("the guest callback failed"),
              std::string::npos);
    EXPECT_TRUE(host.restartSession());
  }
}

TEST(SketchPlugin, AFrameThrowingAnythingFailsTheSessionWithoutEscaping) {
  sigil::test::ScratchDir files("sigil_plugin_frame_exception");
  const auto path = files.path / "scene.dylib";
  publish(SIGIL_SKETCH_FRAME_GUEST, path);
  sketch::Host host(options(path), sketch::test::fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  bool advanced = true;
  EXPECT_NO_THROW(advanced = host.frame(1.0 / 60.0));
  EXPECT_FALSE(advanced);
  EXPECT_NE(host.errorLog().find("unknown exception"), std::string::npos)
      << host.errorLog();
  EXPECT_FALSE(host.frame(1.0 / 60.0));
  ASSERT_TRUE(host.restartSession());
  EXPECT_NO_THROW(advanced = host.frame(1.0 / 60.0));
  EXPECT_FALSE(advanced);
}

TEST(SketchPlugin, AModuleWrittenBeforeItsSidecarIsAPublicationInProgress) {
  sigil::test::ScratchDir files("sigil_plugin_publication");
  const auto path = files.path / "scene.dylib";
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  picture(files.path / "mark.png", 12);
  auto patientOptions = options(path);
  patientOptions.pluginPublicationGrace = std::chrono::hours(1);
  sketch::Host host(std::move(patientOptions), sketch::test::fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  ASSERT_EQ(host.generation(), 1);
  const std::string status = host.status();
  auto* accepted = host.session();

  // A linker writing in place: new bytes beside the sidecar of the last
  // build, which still names this host.
  std::ofstream(path, std::ios::binary | std::ios::app) << "still linking";
  for (int poll = 0; poll < 5; ++poll) {
    host.poll();
    EXPECT_EQ(host.session(), accepted);
    EXPECT_EQ(host.status(), status);
    EXPECT_TRUE(host.errorLog().empty()) << host.errorLog();
  }
  EXPECT_EQ(host.generation(), 1);

  // The sidecar lands, and the next poll adopts the finished module.
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  host.poll();
  EXPECT_TRUE(host.errorLog().empty()) << host.errorLog();
  EXPECT_EQ(host.generation(), 2);
  EXPECT_NE(host.session(), accepted);
}

TEST(SketchPlugin, RefusedModulesConsumeNoGeneration) {
  sigil::test::ScratchDir files("sigil_plugin_generation");
  const auto path = files.path / "scene.dylib";
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  sketch::Host host(options(path), sketch::test::fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  ASSERT_EQ(host.generation(), 1);
  stamp(path, "a different framework build");
  host.poll();
  host.poll();
  EXPECT_EQ(host.generation(), 1);
  EXPECT_NE(host.status().find("keeping previous sketch"), std::string::npos)
      << host.status();
  publish(SIGIL_SKETCH_METADATA_GUEST, path);
  host.poll();
  EXPECT_EQ(host.generation(), 1);
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  host.poll();
  EXPECT_EQ(host.generation(), 2);
}

TEST(SketchPlugin, EachHostAndReplacementGetsItsOwnModuleStatics) {
  sigil::test::ScratchDir files("sigil_plugin_statics");
  const auto path = files.path / "scene.dylib";
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  sketch::Host first(options(path), sketch::test::fonts());
  sketch::Host second(options(path), sketch::test::fonts());
  ASSERT_TRUE(first.live()) << first.errorLog();
  ASSERT_TRUE(second.live()) << second.errorLog();
  EXPECT_EQ(first.canvasSize(), SkSize::Make(1, 1));
  EXPECT_EQ(second.canvasSize(), SkSize::Make(1, 1));
  ASSERT_TRUE(first.restartSession());
  EXPECT_EQ(first.canvasSize(), SkSize::Make(2, 1));
  publish(SIGIL_SKETCH_IMAGE_GUEST, path);
  first.poll();
  EXPECT_EQ(first.canvasSize(), SkSize::Make(1, 1));
  EXPECT_EQ(second.canvasSize(), SkSize::Make(1, 1));
}

TEST(SketchPlugin, ARegularToolchainArtifactCanBeLoaded) {
  const char* path = std::getenv("SIGIL_SKETCH_SDK_TEST_PLUGIN");
  if (!path) GTEST_SKIP() << "external SDK integration supplies the artifact";
  sketch::Host host(options(path), sketch::test::fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  EXPECT_FALSE(host.compiling());
  EXPECT_EQ(host.canvasSize(), SkSize::Make(37, 19));
}

}  // namespace
#endif
