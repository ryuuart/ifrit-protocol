/** @file
 * The signature scan over a blob: whole PNGs are found and bounded
 * exactly, the name before each is recovered, a truncated chunk table
 * ends the scan rather than running off the end, and a limit stops it
 * early.
 */

#include <gtest/gtest.h>
#include <include/core/SkData.h>
#include <sigilimage/asset/Embedded.h>
#include <sigilimage/asset/ImageAsset.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "Pixels.h"

namespace {

using sigil::image::EmbeddedImage;
using sigil::image::embeddedPngs;
using sigil::image::test::assetPath;

/** The bytes of one real PNG, which is what the scan has to bound. */
std::vector<std::byte> onePng() {
  const sk_sp<SkData> data =
      SkData::MakeFromFileName(assetPath("still.png").c_str());
  EXPECT_TRUE(data && data->size() > 8);
  const auto* at = static_cast<const std::byte*>(data->data());
  return {at, at + data->size()};
}

void append(std::vector<std::byte>& into, std::string_view text) {
  for (char letter : text) into.push_back((std::byte)letter);
}

void appendBytes(std::vector<std::byte>& into,
                 const std::vector<std::byte>& from) {
  into.insert(into.end(), from.begin(), from.end());
}

}  // namespace

TEST(Embedded, EveryPngIsFoundAndBoundedExactly) {
  const std::vector<std::byte> png = onePng();
  std::vector<std::byte> blob;
  append(blob, "RIVE\x01");
  append(blob, "home-background");
  blob.push_back(std::byte{0});
  appendBytes(blob, png);
  append(blob, "\x07\x07");
  append(blob, "ui-top-header");
  blob.push_back(std::byte{0});
  appendBytes(blob, png);
  append(blob, "trailing junk");

  const std::vector<EmbeddedImage> found = embeddedPngs(blob);
  ASSERT_EQ(found.size(), 2u);
  EXPECT_EQ(found[0].name, "home-background");
  EXPECT_EQ(found[1].name, "ui-top-header");
  EXPECT_EQ(found[0].length, png.size());
  EXPECT_EQ(found[1].length, png.size());
  // The range is the whole encoded image and nothing after it, which is
  // what makes a decode of the range succeed.
  for (const EmbeddedImage& one : found) {
    ASSERT_LE(one.offset + one.length, blob.size());
    EXPECT_TRUE(sigil::image::ImageAsset::decode(
        SkData::MakeWithCopy(blob.data() + one.offset, one.length)));
  }
}

TEST(Embedded, ATruncatedChunkTableEndsTheScan) {
  std::vector<std::byte> png = onePng();
  std::vector<std::byte> blob;
  appendBytes(blob, png);
  // The second copy loses its tail, so its table runs past the blob.
  blob.insert(blob.end(), png.begin(), png.end() - 20);
  const std::vector<EmbeddedImage> found = embeddedPngs(blob);
  EXPECT_EQ(found.size(), 1u);
}

TEST(Embedded, ALimitStopsTheScanEarlyAndAShortRunIsNotAName) {
  const std::vector<std::byte> png = onePng();
  std::vector<std::byte> blob;
  append(blob, "ab");  // shorter than the minimum: not a name
  appendBytes(blob, png);
  appendBytes(blob, png);
  appendBytes(blob, png);

  EXPECT_EQ(embeddedPngs(blob, {.limit = 2}).size(), 2u);
  const std::vector<EmbeddedImage> found = embeddedPngs(blob);
  ASSERT_EQ(found.size(), 3u);
  EXPECT_TRUE(found[0].name.empty());
}

TEST(Embedded, NothingInNothing) {
  EXPECT_TRUE(embeddedPngs({}).empty());
  const std::vector<std::byte> noise(64, std::byte{0x11});
  EXPECT_TRUE(embeddedPngs(noise).empty());
}
