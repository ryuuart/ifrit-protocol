// SigilLoader behavior: mounts and URI resolution, blob/text loads,
// probe metadata, hot reload, and — with the OpenImageIO backend —
// EXR decode into float SkImages with layer/channel selection.

#include <sigilloader/Loader.h>

#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>

#ifdef SIGILLOADER_HAS_OIIO
#include <OpenImageIO/imageio.h>
#endif

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace sigil::loader;
namespace fs = std::filesystem;

namespace {

struct TempDir {
  fs::path path;
  TempDir() {
    path = fs::temp_directory_path() /
           ("sigilloader_test_" + std::to_string(::getpid()));
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  void write(const std::string &name, std::string_view content) {
    fs::create_directories((path / name).parent_path());
    std::ofstream(path / name, std::ios::binary) << content;
  }
};

} // namespace

TEST(LoaderHub, MountsResolveLongestPrefix) {
  TempDir dir;
  Hub hub;
  hub.mount("res://", dir.path);
  hub.mount("res://deep/", dir.path / "elsewhere");
  EXPECT_EQ(hub.resolve("res://a.txt"), dir.path / "a.txt");
  EXPECT_EQ(hub.resolve("res://deep/b.txt"),
            dir.path / "elsewhere" / "b.txt");
  EXPECT_TRUE(hub.resolve("other://x").empty());
}

TEST(LoaderHub, BlobAndTextLoadThroughMounts) {
  TempDir dir;
  dir.write("notes/hello.txt", "carry the coal");
  Hub hub;
  hub.mount("res://", dir.path);
  auto text = hub.text("res://notes/hello.txt");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, "carry the coal");
  auto bytes = hub.blob("res://notes/hello.txt");
  ASSERT_NE(bytes, nullptr);
  EXPECT_EQ(bytes->bytes.size(), 14u);
  EXPECT_EQ(hub.blob("res://missing.bin"), nullptr);
}

TEST(LoaderHub, MissingFilesHealWithoutStaleCache) {
  TempDir dir;
  Hub hub;
  hub.mount("res://", dir.path);
  EXPECT_EQ(hub.text("res://late.txt"), std::nullopt);
  dir.write("late.txt", "arrived");
  EXPECT_EQ(hub.text("res://late.txt"), "arrived");
}

TEST(LoaderHub, PollReloadsChangedText) {
  TempDir dir;
  dir.write("live.txt", "one");
  Hub hub;
  hub.mount("res://", dir.path);
  EXPECT_EQ(hub.text("res://live.txt"), "one");
  // Filesystem mtime granularity can be coarse; force a distinct stamp.
  dir.write("live.txt", "two");
  fs::last_write_time(dir.path / "live.txt",
                      fs::file_time_type::clock::now() +
                          std::chrono::seconds(2));
  EXPECT_TRUE(hub.poll());
  EXPECT_EQ(hub.text("res://live.txt"), "two");
}

TEST(LoaderHub, ProbeReportsPlainData) {
  TempDir dir;
  dir.write("table.bin", std::string(64, '\0'));
  Hub hub;
  hub.mount("res://", dir.path);
  auto info = hub.probe("res://table.bin");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->kind, ResourceInfo::Kind::Data);
  EXPECT_EQ(info->byteSize, 64u);
}

#ifdef SIGILLOADER_HAS_OIIO

namespace {

/** Writes a tiny EXR with layered channels: default RGBA plus a
 *  "glow" layer whose red channel is 2.5 (HDR range). */
void writeLayeredExr(const fs::path &path) {
  using namespace OIIO;
  constexpr int kSize = 4;
  ImageSpec spec(kSize, kSize, 8, TypeDesc::FLOAT);
  spec.channelnames = {"R", "G", "B", "A",
                       "glow.R", "glow.G", "glow.B", "glow.A"};
  auto out = ImageOutput::create(path.string());
  ASSERT_TRUE(out);
  ASSERT_TRUE(out->open(path.string(), spec));
  std::vector<float> pixels(kSize * kSize * 8);
  for (int px = 0; px < kSize * kSize; ++px) {
    float *p = pixels.data() + px * 8;
    p[0] = 0.25f; p[1] = 0.5f; p[2] = 0.75f; p[3] = 1.0f; // base RGBA
    p[4] = 2.5f;  p[5] = 0.125f; p[6] = 0.0f; p[7] = 1.0f; // glow.*
  }
  ASSERT_TRUE(out->write_image(TypeDesc::FLOAT, pixels.data()));
  ASSERT_TRUE(out->close());
}

} // namespace

TEST(LoaderOiio, ExrDecodesToFloatImage) {
  TempDir dir;
  writeLayeredExr(dir.path / "probe.exr");
  Hub hub;
  hub.mount("res://", dir.path);
  auto image = hub.image("res://probe.exr");
  ASSERT_NE(image, nullptr);
  ASSERT_FALSE(image->frames().empty());
  const sk_sp<SkImage> &sk = image->frames().front().image;
  EXPECT_EQ(sk->width(), 4);
  EXPECT_EQ(sk->colorType(), kRGBA_F32_SkColorType);
}

TEST(LoaderOiio, ExrLayerSelectionReadsHdrChannels) {
  TempDir dir;
  writeLayeredExr(dir.path / "probe.exr");
  Hub hub;
  hub.mount("res://", dir.path);
  auto glow = hub.image("res://probe.exr", {.layer = "glow"});
  ASSERT_NE(glow, nullptr);
  const sk_sp<SkImage> &sk = glow->frames().front().image;
  SkPixmap pixmap;
  ASSERT_TRUE(sk->peekPixels(&pixmap));
  const float *px = (const float *)pixmap.addr(0, 0);
  EXPECT_FLOAT_EQ(px[0], 2.5f);   // HDR value survives (F32)
  EXPECT_FLOAT_EQ(px[1], 0.125f);
}

TEST(LoaderOiio, ProbeListsLayersAndChannels) {
  TempDir dir;
  writeLayeredExr(dir.path / "probe.exr");
  Hub hub;
  hub.mount("res://", dir.path);
  auto info = hub.probe("res://probe.exr");
  ASSERT_TRUE(info.has_value());
  EXPECT_EQ(info->kind, ResourceInfo::Kind::Image);
  EXPECT_EQ(info->format, "openexr");
  EXPECT_EQ(info->image.width, 4);
  EXPECT_TRUE(info->image.floatingPoint);
  EXPECT_EQ(info->image.channels, 8);
  ASSERT_EQ(info->image.layers.size(), 1u);
  EXPECT_EQ(info->image.layers[0], "glow");
}

#endif // SIGILLOADER_HAS_OIIO
