/** @file
 * scry_platform_test — the handlers without a renderer: the surface's
 * pixel format, alignment and resize, the file system's two roots, MIME
 * table and synthesized image-slot files, the logger's level mapping and
 * routing, the staged resource directory and the runtime probe over
 * it.
 */

#include <Ultralight/Buffer.h>
#include <Ultralight/String.h>
#include <gtest/gtest.h>
#include <include/core/SkColorSpace.h>
#include <sigilscry/platform/Runtime.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "FileSystem.h"
#include "Logger.h"
#include "ResourceDir.h"
#include "SkiaSurface.h"

using namespace sigil::scry;

namespace {

std::string utf8(const ultralight::String& s) {
  return std::string(s.utf8().data(), s.utf8().length());
}

std::filesystem::path scratch() {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "sigilscry_platform_test";
  std::filesystem::create_directories(dir);
  return dir;
}

}  // namespace

TEST(ScryPlatform, SurfaceIsPremultipliedBgraWithAlignedRows) {
  SkiaSurface surface(33, 7);
  EXPECT_EQ(surface.width(), 33u);
  EXPECT_EQ(surface.height(), 7u);
  EXPECT_EQ(surface.bitmap().colorType(), kBGRA_8888_SkColorType);
  EXPECT_EQ(surface.bitmap().alphaType(), kPremul_SkAlphaType);
  EXPECT_TRUE(surface.bitmap().colorSpace()->isSRGB());
  EXPECT_EQ(surface.row_bytes() % 16, 0u);
  EXPECT_GE(surface.row_bytes(), 33u * 4u);
  // Skia counts the last row without its padding.
  EXPECT_EQ(surface.size(), surface.row_bytes() * 6u + 33u * 4u);
  void* pixels = surface.LockPixels();
  ASSERT_NE(pixels, nullptr);
  EXPECT_EQ(static_cast<const uint32_t*>(pixels)[0], 0u) << "transparent";
  surface.UnlockPixels();
  // The same size keeps the allocation; a new size takes a new one.
  surface.Resize(33, 7);
  EXPECT_EQ(surface.LockPixels(), pixels);
  surface.Resize(64, 64);
  EXPECT_EQ(surface.width(), 64u);
  EXPECT_EQ(surface.row_bytes(), 256u);
}

TEST(ScryPlatform, SurfaceFactoryMakesAndDestroysSurfaces) {
  SkiaSurfaceFactory factory;
  ultralight::Surface* surface = factory.CreateSurface(8, 4);
  ASSERT_NE(surface, nullptr);
  EXPECT_EQ(surface->width(), 8u);
  EXPECT_EQ(surface->height(), 4u);
  factory.DestroySurface(surface);
}

TEST(ScryPlatform, FileSystemResolvesByTwoRoots) {
  PrefixFileSystem fs("/sdk/resources", "/site", nullptr);
  EXPECT_EQ(fs.resolve("resources/icudt67l.dat"),
            "/sdk/resources/icudt67l.dat");
  EXPECT_EQ(fs.resolve("index.html"), "/site/index.html");
  EXPECT_EQ(fs.resolve("/abs/file.css"), "/abs/file.css");
  EXPECT_EQ(utf8(fs.GetFileCharset("x")), "utf-8");
}

TEST(ScryPlatform, FileSystemKnowsMimeTypes) {
  PrefixFileSystem fs("/sdk", "/site", nullptr);
  EXPECT_EQ(utf8(fs.GetFileMimeType("a/b/page.HTML")), "text/html");
  EXPECT_EQ(utf8(fs.GetFileMimeType("style.css")), "text/css");
  EXPECT_EQ(utf8(fs.GetFileMimeType("app.mjs")), "application/javascript");
  EXPECT_EQ(utf8(fs.GetFileMimeType("font.woff2")), "font/woff2");
  EXPECT_EQ(utf8(fs.GetFileMimeType("gauge.imgsrc")), "image/imgsrc");
  EXPECT_EQ(utf8(fs.GetFileMimeType("thing.xyz")), "application/unknown");
  EXPECT_EQ(utf8(fs.GetFileMimeType("noext")), "application/unknown");
}

TEST(ScryPlatform, FileSystemOpensFilesUnderTheBaseDir) {
  const std::filesystem::path dir = scratch();
  {
    std::ofstream out(dir / "hello.txt", std::ios::binary);
    out << "hello, page";
  }
  PrefixFileSystem fs("/sdk", dir.string(), nullptr);
  EXPECT_TRUE(fs.FileExists("hello.txt"));
  EXPECT_FALSE(fs.FileExists("missing.txt"));
  ultralight::RefPtr<ultralight::Buffer> buffer = fs.OpenFile("hello.txt");
  ASSERT_NE(buffer.get(), nullptr);
  EXPECT_EQ(
      std::string(static_cast<const char*>(buffer->data()), buffer->size()),
      "hello, page");
  EXPECT_EQ(fs.OpenFile("missing.txt").get(), nullptr);
}

TEST(ScryPlatform, FileSystemSynthesizesImageSlotsAndWarnsOnUnknownOnes) {
  std::vector<std::pair<LogLevel, std::string>> log;
  CallbackLogger logger([&](LogLevel level, const std::string& message) {
    log.emplace_back(level, message);
  });
  PrefixFileSystem fs("/sdk", "/site", &logger);
  // Virtual: exists without touching disk, and opens as the indirection
  // file Ultralight expects.
  EXPECT_TRUE(fs.FileExists("gauge.imgsrc"));
  ultralight::RefPtr<ultralight::Buffer> buffer =
      fs.OpenFile("any/gauge.imgsrc");
  ASSERT_NE(buffer.get(), nullptr);
  EXPECT_EQ(
      std::string(static_cast<const char*>(buffer->data()), buffer->size()),
      "IMGSRC-V1\ngauge");
  // No WebImage is registered under that name: a warning names the slot.
  ASSERT_EQ(log.size(), 1u);
  EXPECT_EQ(log[0].first, LogLevel::Warning);
  EXPECT_NE(log[0].second.find("'gauge'"), std::string::npos);
}

TEST(ScryPlatform, LoggerMapsLevelsAndRoutesToTheCallback) {
  std::vector<std::pair<LogLevel, std::string>> log;
  CallbackLogger logger([&](LogLevel level, const std::string& message) {
    log.emplace_back(level, message);
  });
  logger.LogMessage(ultralight::LogLevel::Error, "e");
  logger.LogMessage(ultralight::LogLevel::Warning, "w");
  logger.LogMessage(ultralight::LogLevel::Info, "i");
  logger.log(LogLevel::Info, "own");
  ASSERT_EQ(log.size(), 4u);
  EXPECT_EQ(log[0].first, LogLevel::Error);
  EXPECT_EQ(log[1].first, LogLevel::Warning);
  EXPECT_EQ(log[2].first, LogLevel::Info);
  EXPECT_EQ(log[3].second, "own");
  // Without a callback nothing is stored and nothing throws.
  CallbackLogger quiet({});
  quiet.log(LogLevel::Info, "dropped");
}

TEST(ScryPlatform, ResourceDirIsStagedNextToTheExecutable) {
  const std::string staged = executableAdjacentResourceDir();
  ASSERT_FALSE(staged.empty()) << "ultralight_copy_resources() stages it";
  EXPECT_TRUE(std::filesystem::is_regular_file(std::filesystem::path(staged) /
                                               "icudt67l.dat"));
  // The configured directory wins; otherwise the staged one.
  EXPECT_EQ(resolveResourceDir("/explicit"), "/explicit");
  EXPECT_EQ(resolveResourceDir(""), staged);
  // The probe answers over that same resolved directory: the runtime data
  // is there, so it says yes and writes no reason. A machine missing it
  // gets the reason instead, which is what a caller shows in place of the
  // thing it cannot run.
  std::string why = "untouched";
  EXPECT_TRUE(available(&why));
  EXPECT_EQ(why, "untouched");
  EXPECT_TRUE(available());
}
